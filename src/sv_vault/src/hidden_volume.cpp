#include <sv/vault/hidden_volume.hpp>

#include <sv/container/header_codec.hpp>
#include <sv/container/layout.hpp>
#include <sv/recovery/journal.hpp>
#include <sv/vault/credential_material.hpp>
#include <sv/vault/key_manager.hpp>
#include <sv/vault/manifest.hpp>
#include <sv/vault/vault_engine.hpp>

#include <algorithm>
#include <cstring>

namespace sv::vault {

namespace {

constexpr std::uint64_t kMinHiddenSize = 32ULL * 1024 * 1024;
constexpr std::uint64_t kMinOuterForHidden = 64ULL * 1024 * 1024;
constexpr std::uint64_t kHiddenJournalSize = 4ULL * 1024 * 1024;
constexpr std::uint64_t kHiddenManifestSize = 8ULL * 1024 * 1024;

void set_bitmap_state(std::vector<std::uint8_t>& bitmap, const std::uint64_t chunk_index, const std::uint8_t state) {
    const std::size_t bit_offset = chunk_index * 2;
    const std::size_t byte_index = bit_offset / 8;
    const std::size_t shift = bit_offset % 8;
    if (byte_index >= bitmap.size()) {
        return;
    }
    bitmap[byte_index] = static_cast<std::uint8_t>((bitmap[byte_index] & ~(0x3U << shift)) | ((state & 0x3U) << shift));
}

} 

bool hidden_configured_from_header(const container::HeaderPayload& header) noexcept {
    return header.hidden_max_size > 0;
}

bool hidden_configured_from_manifest(const ManifestState& manifest) noexcept {
    for (const auto& inode : manifest.inodes) {
        if (inode.entry_type == container::InodeEntryType::HiddenReservation) {
            return true;
        }
    }
    return false;
}

void update_hidden_configured(UnlockedVault& session) noexcept {
    if (session.hidden_volume) {
        session.hidden_configured = false;
        return;
    }
    session.hidden_configured = hidden_configured_from_header(session.header)
        || hidden_configured_from_manifest(session.manifest);
}

Result<void> check_write_allowed(const UnlockedVault& session, const std::uint64_t write_end) noexcept {
    if (session.read_only) {
        return Result<void>::err(ErrorCode::InvalidArgument, "vault session is read-only");
    }
    if (!session.hidden_configured || session.hidden_volume) {
        return Result<void>::ok();
    }

    if (session.header.hidden_offset > 0 && write_end > session.header.hidden_offset) {
        return Result<void>::err(ErrorCode::HiddenVolumeProtected, "write crosses hidden volume boundary");
    }

    if (session.protect_hidden && session.header.hidden_offset > 0) {
        const std::uint64_t margin = session.header.protect_margin_bytes;
        const std::uint64_t limit = session.header.hidden_offset > margin
            ? session.header.hidden_offset - margin
            : 0;
        if (write_end > limit) {
            return Result<void>::err(ErrorCode::HiddenVolumeProtected, "write within hidden protection margin");
        }
    }

    return Result<void>::ok();
}

Result<void> create_hidden_volume(
    VaultEngine& engine,
    UnlockedVault& outer_session,
    const CredentialMaterial& hidden_cred,
    const std::uint64_t hidden_max_size) noexcept {
    if (outer_session.hidden_volume) {
        return Result<void>::err(ErrorCode::InvalidArgument, "cannot create hidden volume from hidden session");
    }
    if (outer_session.read_only) {
        return Result<void>::err(ErrorCode::InvalidArgument, "outer session is read-only");
    }
    if (hidden_max_size < kMinHiddenSize) {
        return Result<void>::err(ErrorCode::InvalidArgument, "hidden volume must be at least 32 MiB");
    }
    if (outer_session.header.data_size < kMinOuterForHidden + hidden_max_size) {
        return Result<void>::err(ErrorCode::InvalidArgument, "outer data area too small for hidden volume");
    }
    if (outer_session.header.hidden_max_size > 0) {
        return Result<void>::err(ErrorCode::InvalidArgument, "hidden volume already configured");
    }

    const std::uint64_t hidden_offset = outer_session.header.data_offset + outer_session.header.data_size - hidden_max_size;
    if (hidden_offset <= outer_session.header.data_offset) {
        return Result<void>::err(ErrorCode::InvalidArgument, "invalid hidden offset");
    }

    const std::uint64_t chunk_slot = static_cast<std::uint64_t>(outer_session.header.chunk_size) + crypto::kAeadTagSize;
    const std::uint64_t first_hidden_chunk = (hidden_offset - outer_session.header.data_offset) / chunk_slot;
    const std::uint64_t hidden_chunk_count = chunk_count_for_data(hidden_max_size, outer_session.header.chunk_size);
    const std::uint64_t last_hidden_chunk = first_hidden_chunk + hidden_chunk_count;

    for (const auto& inode : outer_session.manifest.inodes) {
        if (inode.chunk_count == 0) {
            continue;
        }
        if (inode.entry_type != container::InodeEntryType::File) {
            continue;
        }
        const std::uint64_t inode_end_chunk = inode.first_chunk_id + inode.chunk_count;
        if (inode.first_chunk_id < last_hidden_chunk && inode_end_chunk > first_hidden_chunk) {
            return Result<void>::err(
                ErrorCode::InvalidArgument,
                "outer data overlaps planned hidden volume region");
        }
    }

    auto crypto = crypto::create_default_crypto_provider();
    if (!crypto) {
        return Result<void>::err(ErrorCode::PlatformError, "crypto provider unavailable");
    }
    KeyManager key_manager(*crypto);

    const auto hidden_kdf_secret = derive_kdf_secret(hidden_cred);
    if (!hidden_kdf_secret.is_ok()) {
        return Result<void>::err(hidden_kdf_secret.error(), hidden_kdf_secret.message());
    }

    const auto kdf_params = kdf_params_from_blob(outer_session.header.kdf_params_blob);
    std::array<std::uint8_t, 64> kdf_out {};
    auto secret = hidden_kdf_secret.value();
    const auto kdf = crypto->kdf_argon2id(
        secret,
        outer_session.superblock.hidden_salt,
        kdf_params,
        kdf_out);
    crypto->secure_zero(secret);
    if (!kdf.is_ok()) {
        crypto->secure_zero(kdf_out);
        return Result<void>::err(kdf.error(), kdf.message());
    }

    crypto::SecureBuffer hidden_header_key;
    hidden_header_key.resize(32);
    std::memcpy(hidden_header_key.data(), kdf_out.data(), 32);
    crypto->secure_zero(kdf_out);

    const auto hidden_confirm = key_manager.compute_header_key_confirm(hidden_header_key.span());
    if (!hidden_confirm.is_ok()) {
        return Result<void>::err(hidden_confirm.error(), hidden_confirm.message());
    }

    const std::uint64_t hidden_data_size = hidden_max_size - kHiddenJournalSize - kHiddenManifestSize;
    if (hidden_data_size < kMinHiddenSize / 2) {
        return Result<void>::err(ErrorCode::InvalidArgument, "hidden region too small after journal/manifest");
    }

    container::HeaderPayload hidden_payload {};
    hidden_payload.slot_type = static_cast<std::uint8_t>(container::HeaderSlotType::Hidden);
    const auto vmk_random = crypto->random_bytes(hidden_payload.volume_master_key);
    if (!vmk_random.is_ok()) {
        return Result<void>::err(vmk_random.error(), vmk_random.message());
    }
    hidden_payload.header_key_confirm = hidden_confirm.value();
    hidden_payload.manifest_offset = hidden_offset + kHiddenJournalSize;
    hidden_payload.manifest_size = kHiddenManifestSize;
    hidden_payload.journal_offset = hidden_offset;
    hidden_payload.journal_size = kHiddenJournalSize;
    hidden_payload.data_offset = hidden_offset + kHiddenJournalSize + kHiddenManifestSize;
    hidden_payload.data_size = hidden_data_size;
    hidden_payload.chunk_size = outer_session.header.chunk_size;
    encode_kdf_params_blob(kdf_params, hidden_payload.kdf_params_blob);
    hidden_payload.manifest_generation = 1;
    hidden_payload.hidden_journal_offset = 0;
    hidden_payload.hidden_journal_size = kHiddenJournalSize;
    hidden_payload.hidden_manifest_offset = kHiddenJournalSize;
    hidden_payload.hidden_manifest_size = kHiddenManifestSize;
    hidden_payload.hidden_data_offset = kHiddenJournalSize + kHiddenManifestSize;
    hidden_payload.hidden_data_size = hidden_data_size;

    const auto hidden_manifest = create_empty_manifest(hidden_data_size, hidden_payload.chunk_size);
    if (!hidden_manifest.is_ok()) {
        return Result<void>::err(hidden_manifest.error(), hidden_manifest.message());
    }
    hidden_payload.manifest_root_hash = hidden_manifest.value().root_hash;

    const auto hidden_sector = container::seal_header_sector(
        *crypto,
        hidden_payload,
        hidden_header_key.span(),
        outer_session.superblock,
        container::HeaderSlotId::Hidden,
        container::HeaderSlotType::Hidden);
    if (!hidden_sector.is_ok()) {
        return Result<void>::err(hidden_sector.error(), hidden_sector.message());
    }

    const auto write_hidden = outer_session.container.write_header_sector(
        outer_session.superblock.hidden_header_offset,
        hidden_sector.value());
    if (!write_hidden.is_ok()) {
        return write_hidden;
    }

    outer_session.header.hidden_offset = hidden_offset;
    outer_session.header.hidden_max_size = hidden_max_size;
    outer_session.header.hidden_present = 1;
    outer_session.header.hidden_journal_offset = 0;
    outer_session.header.hidden_journal_size = kHiddenJournalSize;
    outer_session.header.hidden_manifest_offset = kHiddenJournalSize;
    outer_session.header.hidden_manifest_size = kHiddenManifestSize;
    outer_session.header.hidden_data_offset = kHiddenJournalSize + kHiddenManifestSize;
    outer_session.header.hidden_data_size = hidden_data_size;
    outer_session.superblock.flags |= container::SUPERBLOCK_FLAG_HIDDEN_CONFIGURED;

    for (std::uint64_t chunk = first_hidden_chunk; chunk < last_hidden_chunk; ++chunk) {
        set_bitmap_state(outer_session.manifest.bitmap, chunk, 2);
    }

    if (outer_session.manifest.inodes.size() >= container::SVLT_MAX_INODES_V1) {
        return Result<void>::err(ErrorCode::ManifestFull, "inode table full");
    }

    Inode reservation {};
    reservation.entry_type = container::InodeEntryType::HiddenReservation;
    reservation.name_utf8 = "__sv_hidden_reserve__";
    reservation.parent_id = 0;
    reservation.inode_id = std::max_element(
                               outer_session.manifest.inodes.begin(),
                               outer_session.manifest.inodes.end(),
                               [](const Inode& a, const Inode& b) { return a.inode_id < b.inode_id; })
                               ->inode_id
        + 1;
    outer_session.manifest.inodes.push_back(reservation);
    std::sort(outer_session.manifest.inodes.begin(), outer_session.manifest.inodes.end(), inode_less);

    recovery::Journal hidden_journal(
        outer_session.container,
        *crypto,
        hidden_payload.volume_master_key,
        hidden_offset,
        kHiddenJournalSize);
    const auto journal_init = hidden_journal.initialize();
    if (!journal_init.is_ok()) {
        return journal_init;
    }

    const auto pages = manifest_to_pages(hidden_manifest.value());
    if (!pages.is_ok()) {
        return Result<void>::err(pages.error(), pages.message());
    }
    for (std::size_t page_index = 0; page_index < 2; ++page_index) {
        const ManifestPage& page = page_index == 0 ? pages.value().first : pages.value().second;
        std::vector<std::uint8_t> slot(kManifestPageSlotSize);
        const auto enc = encrypt_manifest_page(
            *crypto,
            hidden_payload.volume_master_key,
            page,
            Span<std::uint8_t>(slot.data(), kManifestMaxPagePlaintext),
            Span<std::uint8_t>(slot.data() + kManifestMaxPagePlaintext, crypto::kAeadTagSize));
        if (!enc.is_ok()) {
            return enc;
        }

        const std::uint64_t offset = hidden_payload.manifest_offset + page_index * kManifestPageSlotSize;
        const auto write_page = outer_session.container.write_region(offset, as_bytes(slot));
        if (!write_page.is_ok()) {
            return write_page;
        }
    }

    update_hidden_configured(outer_session);
    return engine.commit_manifest(outer_session);
}

} 