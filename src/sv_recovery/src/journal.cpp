#include <sv/recovery/journal.hpp>

#include <sv/container/superblock.hpp>
#include <sv/crc32.hpp>
#include <sv/crypto/hkdf.hpp>
#include <sv/endian.hpp>

#include <sodium.h>

#include <algorithm>
#include <cstring>


namespace sv::recovery {

namespace {

constexpr std::size_t kJournalSuperblockSize = 512;
constexpr std::size_t kJournalRecordsBase = 512;
constexpr std::size_t kMaxRecordSize = 65536;
constexpr std::size_t kRecordPlainHeaderSize = 16;

[[nodiscard]] std::array<std::uint8_t, 24> derive_journal_nonce(const std::uint64_t sequence) noexcept {
    std::array<std::uint8_t, 16> input {};
    std::memcpy(input.data(), "sv-jn-v1", 8);
    write_le64(input.data() + 8, sequence);

    std::array<std::uint8_t, 24> nonce {};
    crypto_generichash_blake2b(nonce.data(), nonce.size(), input.data(), input.size(), nullptr, 0);
    return nonce;
}

[[nodiscard]] std::uint64_t align_up(const std::uint64_t value, const std::uint64_t alignment) noexcept {
    return (value + alignment - 1) / alignment * alignment;
}

} 

Journal::Journal(
    container::ContainerFileStore& container,
    crypto::CryptoProvider& crypto,
    const Span<const std::uint8_t> vmk,
    const std::uint64_t journal_offset,
    const std::uint64_t journal_size) noexcept
    : container_(container), crypto_(crypto), vmk_(vmk), journal_offset_(journal_offset), journal_size_(journal_size) {}

Result<JournalSuperblock> Journal::read_superblock() noexcept {
    const auto data = container_.read_region(journal_offset_, kJournalSuperblockSize);
    if (!data.is_ok()) {
        return Result<JournalSuperblock>::err(data.error(), data.message());
    }

    const bool all_zero = std::all_of(data.value().begin(), data.value().end(), [](const std::uint8_t b) {
        return b == 0;
    });
    if (all_zero) {
        JournalSuperblock empty {};
        return Result<JournalSuperblock>::ok(empty);
    }

    JournalSuperblock sb {};
    std::memcpy(sb.magic.data(), data.value().data(), 4);
    sb.version = read_le32(data.value().data() + 4);
    sb.write_pointer = read_le64(data.value().data() + 8);
    sb.last_committed_seq = read_le64(data.value().data() + 16);
    sb.first_record_offset = read_le64(data.value().data() + 24);
    sb.crc32 = read_le32(data.value().data() + 32);

    const std::uint32_t expected = crc32_ieee(Span<const std::uint8_t>(data.value().data(), 32));
    if (sb.crc32 != expected) {
        return Result<JournalSuperblock>::err(ErrorCode::CorruptContainer, "journal superblock crc mismatch");
    }
    if (sb.magic[0] != 'S' || sb.magic[1] != 'V' || sb.magic[2] != 'J' || sb.magic[3] != 'R') {
        return Result<JournalSuperblock>::err(ErrorCode::CorruptContainer, "invalid journal magic");
    }
    return Result<JournalSuperblock>::ok(sb);
}

Result<void> Journal::write_container_superblock_flags(const container::Superblock& superblock) noexcept {
    return container_.write_superblock_both_no_sync(superblock);
}

Result<void> Journal::write_superblock(const JournalSuperblock& sb) noexcept {
    std::array<std::uint8_t, kJournalSuperblockSize> encoded {};
    std::memcpy(encoded.data(), sb.magic.data(), 4);
    write_le32(encoded.data() + 4, sb.version);
    write_le64(encoded.data() + 8, sb.write_pointer);
    write_le64(encoded.data() + 16, sb.last_committed_seq);
    write_le64(encoded.data() + 24, sb.first_record_offset);
    const std::uint32_t crc = crc32_ieee(Span<const std::uint8_t>(encoded.data(), 32));
    write_le32(encoded.data() + 32, crc);
    return container_.write_region(journal_offset_, as_bytes(encoded));
}

Result<void> Journal::initialize() noexcept {
    JournalSuperblock sb {};
    sb.write_pointer = 0;
    sb.last_committed_seq = 0;
    sb.first_record_offset = 0;
    return write_superblock(sb);
}

Result<std::uint64_t> Journal::append_record(
    JournalSuperblock& jsb,
    const JournalOpType op_type,
    const Span<const std::uint8_t> payload) noexcept {
    if (payload.size() > kMaxRecordSize - kRecordPlainHeaderSize - crypto::kAeadTagSize - 4) {
        return Result<std::uint64_t>::err(ErrorCode::InvalidArgument, "journal record payload too large");
    }

    const std::uint64_t sequence = jsb.last_committed_seq + 1 + open_sequence_;
    ++open_sequence_;

    std::vector<std::uint8_t> plaintext(kRecordPlainHeaderSize + payload.size());
    write_le64(plaintext.data(), sequence);
    write_le16(plaintext.data() + 8, static_cast<std::uint16_t>(op_type));
    write_le16(plaintext.data() + 10, 0);
    write_le32(plaintext.data() + 12, static_cast<std::uint32_t>(payload.size()));
    if (!payload.empty()) {
        std::memcpy(plaintext.data() + kRecordPlainHeaderSize, payload.data(), payload.size());
    }

    std::vector<std::uint8_t> encrypted(plaintext.size() + crypto::kAeadTagSize);
    const auto key = crypto::hkdf_journal_key(vmk_, sequence);
    if (!key.is_ok()) {
        return Result<std::uint64_t>::err(key.error(), key.message());
    }
    crypto::AeadNonce nonce {};
    const auto derived_nonce = derive_journal_nonce(sequence);
    std::memcpy(nonce.bytes.data(), derived_nonce.data(), derived_nonce.size());

    const auto enc = crypto_.aead_encrypt(
        plaintext,
        Span<const std::uint8_t> {},
        key.value(),
        nonce,
        Span<std::uint8_t>(encrypted.data(), plaintext.size()),
        Span<std::uint8_t>(encrypted.data() + plaintext.size(), crypto::kAeadTagSize));
    if (!enc.is_ok()) {
        return Result<std::uint64_t>::err(enc.error(), enc.message());
    }

    const std::uint32_t record_len = static_cast<std::uint32_t>(4 + encrypted.size());
    const std::uint64_t padded_len = align_up(record_len, 8);
    
    
    if (kJournalRecordsBase + jsb.write_pointer + padded_len > journal_size_
        && jsb.write_pointer == jsb.first_record_offset
        && jsb.write_pointer != 0) {
        jsb.write_pointer = 0;
        jsb.first_record_offset = 0;
    }
    if (kJournalRecordsBase + jsb.write_pointer + padded_len > journal_size_) {
        return Result<std::uint64_t>::err(ErrorCode::IoError, "journal region full");
    }

    std::vector<std::uint8_t> record(static_cast<std::size_t>(padded_len), 0);
    write_le32(record.data(), record_len);
    std::memcpy(record.data() + 4, encrypted.data(), encrypted.size());

    const std::uint64_t record_offset = journal_offset_ + kJournalRecordsBase + jsb.write_pointer;
    const auto write = container_.write_region(record_offset, as_bytes(record));
    if (!write.is_ok()) {
        return Result<std::uint64_t>::err(write.error(), write.message());
    }

    jsb.write_pointer += padded_len;
    const auto persist = write_superblock(jsb);
    if (!persist.is_ok()) {
        return Result<std::uint64_t>::err(persist.error(), persist.message());
    }

    return Result<std::uint64_t>::ok(sequence);
}

Result<void> Journal::append_begin(
    const std::array<std::uint8_t, 16>& transaction_id,
    container::Superblock* superblock) noexcept {
    auto jsb = read_superblock();
    if (!jsb.is_ok()) {
        return Result<void>::err(jsb.error(), jsb.message());
    }

    open_sequence_ = 0;
    JournalSuperblock state = jsb.value();
    const auto seq = append_record(
        state,
        JournalOpType::Begin,
        Span<const std::uint8_t>(transaction_id.data(), transaction_id.size()));
    if (!seq.is_ok()) {
        return Result<void>::err(seq.error(), seq.message());
    }

    if (superblock == nullptr) {
        return Result<void>::ok();
    }
    superblock->flags |= container::SUPERBLOCK_FLAG_JOURNAL_DIRTY;
    return write_container_superblock_flags(*superblock);
}

Result<void> Journal::append_commit(
    const std::uint64_t manifest_generation,
    const Span<const std::uint8_t> manifest_root_hash,
    container::Superblock* superblock) noexcept {
    if (manifest_root_hash.size() != 32) {
        return Result<void>::err(ErrorCode::InvalidArgument, "manifest_root_hash must be 32 bytes");
    }

    auto jsb = read_superblock();
    if (!jsb.is_ok()) {
        return Result<void>::err(jsb.error(), jsb.message());
    }

    std::array<std::uint8_t, 40> payload {};
    write_le64(payload.data(), manifest_generation);
    std::memcpy(payload.data() + 8, manifest_root_hash.data(), 32);

    JournalSuperblock state = jsb.value();
    const auto seq = append_record(
        state,
        JournalOpType::Commit,
        Span<const std::uint8_t>(payload.data(), payload.size()));
    if (!seq.is_ok()) {
        return Result<void>::err(seq.error(), seq.message());
    }

    jsb = read_superblock();
    if (!jsb.is_ok()) {
        return Result<void>::err(jsb.error(), jsb.message());
    }
    JournalSuperblock updated = jsb.value();
    updated.last_committed_seq = seq.value();
    
    updated.write_pointer = 0;
    updated.first_record_offset = 0;
    const auto persist = write_superblock(updated);
    if (!persist.is_ok()) {
        return persist;
    }

    open_sequence_ = 0;
    if (superblock == nullptr) {
        return Result<void>::ok();
    }
    superblock->flags &= ~container::SUPERBLOCK_FLAG_JOURNAL_DIRTY;
    return write_container_superblock_flags(*superblock);
}

Result<void> Journal::append_abort(container::Superblock* superblock) noexcept {
    auto jsb = read_superblock();
    if (!jsb.is_ok()) {
        return Result<void>::err(jsb.error(), jsb.message());
    }

    JournalSuperblock state = jsb.value();
    const auto seq = append_record(state, JournalOpType::Abort, Span<const std::uint8_t> {});
    if (!seq.is_ok()) {
        return Result<void>::err(seq.error(), seq.message());
    }

    jsb = read_superblock();
    if (!jsb.is_ok()) {
        return Result<void>::err(jsb.error(), jsb.message());
    }
    JournalSuperblock updated = jsb.value();
    
    updated.write_pointer = 0;
    updated.first_record_offset = 0;
    const auto persist = write_superblock(updated);
    if (!persist.is_ok()) {
        return persist;
    }

    open_sequence_ = 0;
    if (superblock == nullptr) {
        return Result<void>::ok();
    }
    superblock->flags &= ~container::SUPERBLOCK_FLAG_JOURNAL_DIRTY;
    return write_container_superblock_flags(*superblock);
}

Result<void> Journal::replay_if_dirty(container::Superblock* superblock, const bool force_check) noexcept {
    if (!force_check) {
        if (superblock == nullptr) {
            return Result<void>::ok();
        }
        if ((superblock->flags & container::SUPERBLOCK_FLAG_JOURNAL_DIRTY) == 0) {
            return Result<void>::ok();
        }
    }

    const auto jsb = read_superblock();
    if (!jsb.is_ok()) {
        return Result<void>::err(jsb.error(), jsb.message());
    }

    std::uint64_t offset = jsb.value().first_record_offset;
    bool saw_begin = false;
    bool saw_commit = false;

    while (offset < jsb.value().write_pointer) {
        const auto record_data = container_.read_region(
            journal_offset_ + kJournalRecordsBase + offset,
            std::min<std::size_t>(kMaxRecordSize, static_cast<std::size_t>(jsb.value().write_pointer - offset)));
        if (!record_data.is_ok()) {
            return Result<void>::err(record_data.error(), record_data.message());
        }
        if (record_data.value().size() < 4) {
            break;
        }

        const std::uint32_t record_len = read_le32(record_data.value().data());
        if (record_len < 4 + crypto::kAeadTagSize + kRecordPlainHeaderSize || record_len > record_data.value().size()) {
            return Result<void>::err(ErrorCode::TransactionIncomplete, "invalid journal record length");
        }

        const std::size_t body_len = record_len - 4;
        const auto ciphertext = Span<const std::uint8_t>(record_data.value().data() + 4, body_len - crypto::kAeadTagSize);
        const auto tag = Span<const std::uint8_t>(
            record_data.value().data() + 4 + body_len - crypto::kAeadTagSize,
            crypto::kAeadTagSize);

        std::vector<std::uint8_t> plaintext(ciphertext.size());
        bool decrypted = false;
        JournalOpType op_type = JournalOpType::Abort;
        for (std::uint64_t seq = jsb.value().last_committed_seq + 1; seq <= jsb.value().last_committed_seq + 64; ++seq) {
            const auto key = crypto::hkdf_journal_key(vmk_, seq);
            if (!key.is_ok()) {
                continue;
            }
            crypto::AeadNonce nonce {};
            const auto derived_nonce = derive_journal_nonce(seq);
            std::memcpy(nonce.bytes.data(), derived_nonce.data(), derived_nonce.size());
            const auto dec = crypto_.aead_decrypt(
                ciphertext,
                Span<const std::uint8_t> {},
                key.value(),
                nonce,
                tag,
                plaintext);
            if (dec.is_ok()) {
                const std::uint64_t record_seq = read_le64(plaintext.data());
                if (record_seq != seq) {
                    continue;
                }
                op_type = static_cast<JournalOpType>(read_le16(plaintext.data() + 8));
                decrypted = true;
                break;
            }
        }
        if (!decrypted) {
            return Result<void>::err(ErrorCode::TransactionIncomplete, "journal record decrypt failed");
        }

        if (op_type == JournalOpType::Begin) {
            saw_begin = true;
            saw_commit = false;
        } else if (op_type == JournalOpType::Commit) {
            saw_commit = true;
        } else if (op_type == JournalOpType::Abort) {
            saw_begin = false;
            saw_commit = false;
        }

        offset += align_up(record_len, 8);
    }

    if (saw_begin && !saw_commit) {
        return append_abort(superblock);
    }

    if (superblock == nullptr) {
        return Result<void>::ok();
    }
    superblock->flags &= ~container::SUPERBLOCK_FLAG_JOURNAL_DIRTY;
    return write_container_superblock_flags(*superblock);
}

} 