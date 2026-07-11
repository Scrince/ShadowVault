#include <sv/vault/chunk_store.hpp>

#include <sv/crypto/hkdf.hpp>
#include <sv/endian.hpp>

#include <sodium.h>

#include <cstring>

namespace sv::vault {

ChunkStore::ChunkStore(
    container::ContainerFileStore& container,
    crypto::CryptoProvider& crypto,
    const std::uint64_t data_offset,
    const std::uint32_t chunk_size) noexcept
    : container_(container), crypto_(crypto), data_offset_(data_offset), chunk_size_(chunk_size) {}

std::uint64_t ChunkStore::chunk_disk_offset(const std::uint64_t chunk_id) const noexcept {
    return data_offset_ + chunk_id * (static_cast<std::uint64_t>(chunk_size_) + crypto::kAeadTagSize);
}

Result<std::array<std::uint8_t, 24>> ChunkStore::derive_chunk_nonce(
    const Span<const std::uint8_t> base_nonce,
    const std::uint32_t chunk_index) const noexcept {
    std::array<std::uint8_t, 32> input {};
    std::memcpy(input.data(), base_nonce.data(), base_nonce.size());
    write_le64(input.data() + base_nonce.size(), chunk_index);

    std::array<std::uint8_t, 24> nonce {};
    crypto_generichash_blake2b(
        nonce.data(),
        nonce.size(),
        input.data(),
        base_nonce.size() + 8,
        nullptr,
        0);
    return Result<std::array<std::uint8_t, 24>>::ok(nonce);
}

Result<void> ChunkStore::encrypt_chunk(
    const Span<const std::uint8_t> vmk,
    const std::uint64_t chunk_id,
    const std::uint64_t inode_id,
    const std::uint32_t chunk_index,
    const std::uint64_t manifest_generation,
    const Span<const std::uint8_t> base_nonce,
    const Span<const std::uint8_t> plaintext,
    const Span<std::uint8_t> ciphertext_out,
    const Span<std::uint8_t> tag_out) const {
    const auto nonce = derive_chunk_nonce(base_nonce, chunk_index);
    if (!nonce.is_ok()) {
        return Result<void>::err(nonce.error(), nonce.message());
    }

    std::array<std::uint8_t, 16> aad {};
    write_le64(aad.data(), inode_id);
    write_le32(aad.data() + 8, chunk_index);
    write_le64(aad.data() + 12, manifest_generation);

    const auto key = crypto::hkdf_chunk_key(vmk, chunk_id);
    crypto::AeadNonce aead_nonce {};
    std::memcpy(aead_nonce.bytes.data(), nonce.value().data(), nonce.value().size());

    return crypto_.aead_encrypt(plaintext, aad, key, aead_nonce, ciphertext_out, tag_out);
}

Result<std::vector<std::uint8_t>> ChunkStore::decrypt_chunk(
    const Span<const std::uint8_t> vmk,
    const std::uint64_t chunk_id,
    const std::uint64_t inode_id,
    const std::uint32_t chunk_index,
    const std::uint64_t manifest_generation,
    const Span<const std::uint8_t> base_nonce,
    const std::size_t plaintext_size,
    const Span<const std::uint8_t> ciphertext,
    const Span<const std::uint8_t> tag) const {
    const auto nonce = derive_chunk_nonce(base_nonce, chunk_index);
    if (!nonce.is_ok()) {
        return Result<std::vector<std::uint8_t>>::err(nonce.error(), nonce.message());
    }

    std::array<std::uint8_t, 16> aad {};
    write_le64(aad.data(), inode_id);
    write_le32(aad.data() + 8, chunk_index);
    write_le64(aad.data() + 12, manifest_generation);

    const auto key = crypto::hkdf_chunk_key(vmk, chunk_id);
    crypto::AeadNonce aead_nonce {};
    std::memcpy(aead_nonce.bytes.data(), nonce.value().data(), nonce.value().size());

    std::vector<std::uint8_t> plaintext(plaintext_size);
    const auto dec = crypto_.aead_decrypt(ciphertext, aad, key, aead_nonce, tag, plaintext);
    if (!dec.is_ok()) {
        return Result<std::vector<std::uint8_t>>::err(dec.error(), dec.message());
    }
    return Result<std::vector<std::uint8_t>>::ok(std::move(plaintext));
}

Result<std::vector<std::uint8_t>> ChunkStore::read_file(
    const Inode& inode,
    const Span<const std::uint8_t> vmk,
    const std::uint64_t manifest_generation) const {
    if (inode.chunk_count == 0) {
        return Result<std::vector<std::uint8_t>>::ok({});
    }

    std::vector<std::uint8_t> output;
    output.reserve(static_cast<std::size_t>(inode.size));

    for (std::uint32_t index = 0; index < inode.chunk_count; ++index) {
        const std::uint64_t chunk_id = inode.first_chunk_id + index;
        const std::uint64_t offset = chunk_disk_offset(chunk_id);
        const auto raw = container_.read_region(offset, chunk_size_ + crypto::kAeadTagSize);
        if (!raw.is_ok()) {
            return Result<std::vector<std::uint8_t>>::err(raw.error(), raw.message());
        }

        const std::size_t plain_size = (index + 1 == inode.chunk_count)
            ? static_cast<std::size_t>(inode.size - static_cast<std::uint64_t>(index) * chunk_size_)
            : chunk_size_;

        const auto plain = decrypt_chunk(
            vmk,
            chunk_id,
            inode.inode_id,
            index,
            manifest_generation,
            inode.base_nonce,
            chunk_size_,
            Span<const std::uint8_t>(raw.value().data(), chunk_size_),
            Span<const std::uint8_t>(raw.value().data() + chunk_size_, crypto::kAeadTagSize));
        if (!plain.is_ok()) {
            return Result<std::vector<std::uint8_t>>::err(plain.error(), plain.message());
        }
        if (plain.value().size() < plain_size) {
            return Result<std::vector<std::uint8_t>>::err(ErrorCode::CorruptContainer, "decrypted chunk shorter than expected");
        }
        output.insert(output.end(), plain.value().begin(), plain.value().begin() + static_cast<std::ptrdiff_t>(plain_size));
    }

    return Result<std::vector<std::uint8_t>>::ok(std::move(output));
}

Result<ChunkWriteResult> ChunkStore::write_file(
    const Inode& inode,
    const Span<const std::uint8_t> vmk,
    const std::uint64_t manifest_generation,
    const Span<const std::uint8_t> data,
    const std::uint64_t first_chunk_id) {
    const std::uint32_t chunk_count = data.empty()
        ? 0
        : static_cast<std::uint32_t>((data.size() + chunk_size_ - 1) / chunk_size_);

    for (std::uint32_t index = 0; index < chunk_count; ++index) {
        const std::size_t offset = static_cast<std::size_t>(index) * chunk_size_;
        const std::size_t size = std::min<std::size_t>(chunk_size_, data.size() - offset);

        std::vector<std::uint8_t> chunk_plain(chunk_size_, 0);
        std::memcpy(chunk_plain.data(), data.data() + offset, size);
        if (size < chunk_size_) {
            const std::uint8_t pad = static_cast<std::uint8_t>(chunk_size_ - size);
            for (std::size_t i = size; i < chunk_size_; ++i) {
                chunk_plain[i] = pad;
            }
        }

        std::vector<std::uint8_t> on_disk(chunk_size_ + crypto::kAeadTagSize);
        const auto enc = encrypt_chunk(
            vmk,
            first_chunk_id + index,
            inode.inode_id,
            index,
            manifest_generation,
            inode.base_nonce,
            chunk_plain,
            Span<std::uint8_t>(on_disk.data(), chunk_size_),
            Span<std::uint8_t>(on_disk.data() + chunk_size_, crypto::kAeadTagSize));
        if (!enc.is_ok()) {
            return Result<ChunkWriteResult>::err(enc.error(), enc.message());
        }

        const auto write = container_.write_region(
            chunk_disk_offset(first_chunk_id + index),
            as_bytes(on_disk));
        if (!write.is_ok()) {
            return Result<ChunkWriteResult>::err(write.error(), write.message());
        }
    }

    ChunkWriteResult result {};
    result.chunk_count = chunk_count;
    result.first_chunk_id = first_chunk_id;
    return Result<ChunkWriteResult>::ok(result);
}

} // namespace sv::vault