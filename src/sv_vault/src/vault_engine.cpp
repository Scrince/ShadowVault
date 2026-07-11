#include <sv/vault/vault_engine.hpp>

#include <sv/container/header_codec.hpp>
#include <sv/container/layout.hpp>
#include <sv/container/superblock.hpp>
#include <sv/recovery/journal.hpp>
#include <sv/vault/chunk_store.hpp>
#include <sv/vault/hidden_volume.hpp>

#include <sodium.h>

#include <algorithm>
#include <cstring>

namespace sv::vault {

namespace {

std::filesystem::path normalize_vault_path(std::filesystem::path path) {
    path = path.lexically_normal();
    std::string generic = path.generic_string();
    while (!generic.empty() && generic.front() == '/') {
        generic.erase(generic.begin());
    }
    if (generic.empty()) {
        return std::filesystem::path("/");
    }
    return std::filesystem::path(generic);
}

std::uint64_t unix_now() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::uint64_t session_journal_offset(const UnlockedVault& session) noexcept {
    return session.hidden_volume ? session.header.journal_offset : session.superblock.journal_offset;
}

std::uint64_t session_journal_size(const UnlockedVault& session) noexcept {
    return session.hidden_volume ? session.header.journal_size : session.superblock.journal_size;
}

void set_bitmap_state(std::vector<std::uint8_t>& bitmap, std::uint64_t chunk_index, std::uint8_t state) {
    const std::size_t bit_offset = chunk_index * 2;
    const std::size_t byte_index = bit_offset / 8;
    const std::size_t shift = bit_offset % 8;
    if (byte_index >= bitmap.size()) {
        return;
    }
    bitmap[byte_index] = static_cast<std::uint8_t>((bitmap[byte_index] & ~(0x3U << shift)) | ((state & 0x3U) << shift));
}

[[nodiscard]] std::uint8_t get_bitmap_state(const std::vector<std::uint8_t>& bitmap, std::uint64_t chunk_index) {
    const std::size_t bit_offset = chunk_index * 2;
    const std::size_t byte_index = bit_offset / 8;
    const std::size_t shift = bit_offset % 8;
    if (byte_index >= bitmap.size()) {
        return 0;
    }
    return static_cast<std::uint8_t>((bitmap[byte_index] >> shift) & 0x3U);
}

void free_file_chunks(ManifestState& manifest, const Inode& inode) {
    for (std::uint32_t i = 0; i < inode.chunk_count; ++i) {
        set_bitmap_state(manifest.bitmap, inode.first_chunk_id + i, 0);
    }
}

} // namespace

VaultEngine::VaultEngine()
    : crypto_(crypto::create_default_crypto_provider()), key_manager_(*crypto_) {}

Result<ManifestState> VaultEngine::read_manifest(
    container::ContainerFileStore& container,
    const container::HeaderPayload& header,
    const Span<const std::uint8_t> vmk) const {
    ManifestPage inode_page {};
    ManifestPage bitmap_page {};

    for (std::uint64_t page_index = 0; page_index < 2; ++page_index) {
        const std::uint64_t slot_offset = header.manifest_offset + page_index * kManifestPageSlotSize;
        const auto slot = container.read_region(slot_offset, kManifestPageSlotSize);
        if (!slot.is_ok()) {
            return Result<ManifestState>::err(slot.error(), slot.message());
        }

        const auto page = decrypt_manifest_page(
            *crypto_,
            vmk,
            Span<const std::uint8_t>(slot.value().data(), slot.value().size() - crypto::kAeadTagSize),
            Span<const std::uint8_t>(slot.value().data() + slot.value().size() - crypto::kAeadTagSize, crypto::kAeadTagSize),
            header.manifest_generation,
            page_index);
        if (!page.is_ok()) {
            return Result<ManifestState>::err(page.error(), page.message());
        }

        if (page_index == 0) {
            inode_page = page.value();
        } else {
            bitmap_page = page.value();
        }
    }

    return manifest_from_pages(inode_page, bitmap_page);
}

Result<void> VaultEngine::write_manifest(UnlockedVault& session, const bool bump_generation) {
    recovery::Journal journal(
        session.container,
        *crypto_,
        session.vmk.span(),
        session_journal_offset(session),
        session_journal_size(session));

    std::array<std::uint8_t, 16> transaction_id {};
    const auto txn_random = crypto_->random_bytes(transaction_id);
    if (!txn_random.is_ok()) {
        return Result<void>::err(txn_random.error(), txn_random.message());
    }

    container::Superblock* dirty_tracker =
        session.hidden_volume ? nullptr : &session.superblock;

    const auto begin = journal.append_begin(transaction_id, dirty_tracker);
    if (!begin.is_ok()) {
        return begin;
    }

    if (bump_generation) {
        session.manifest.generation += 1;
    }
    const auto hash = compute_manifest_root_hash(session.manifest);
    if (!hash.is_ok()) {
        (void)journal.append_abort(dirty_tracker);
        return Result<void>::err(hash.error(), hash.message());
    }
    session.manifest.root_hash = hash.value();
    session.header.manifest_generation = session.manifest.generation;
    session.header.manifest_root_hash = session.manifest.root_hash;

    const auto pages = manifest_to_pages(session.manifest);
    if (!pages.is_ok()) {
        (void)journal.append_abort(dirty_tracker);
        return Result<void>::err(pages.error(), pages.message());
    }

    for (std::size_t page_index = 0; page_index < 2; ++page_index) {
        const ManifestPage& page = page_index == 0 ? pages.value().first : pages.value().second;
        std::vector<std::uint8_t> slot(kManifestPageSlotSize);
        const auto enc = encrypt_manifest_page(
            *crypto_,
            session.vmk.span(),
            page,
            Span<std::uint8_t>(slot.data(), kManifestMaxPagePlaintext),
            Span<std::uint8_t>(slot.data() + kManifestMaxPagePlaintext, crypto::kAeadTagSize));
        if (!enc.is_ok()) {
            (void)journal.append_abort(dirty_tracker);
            return enc;
        }

        const std::uint64_t offset = session.header.manifest_offset + page_index * kManifestPageSlotSize;
        const auto write = session.container.write_region(offset, as_bytes(slot));
        if (!write.is_ok()) {
            (void)journal.append_abort(dirty_tracker);
            return write;
        }
    }

    const auto commit = journal.append_commit(
        session.manifest.generation,
        session.manifest.root_hash,
        dirty_tracker);
    if (!commit.is_ok()) {
        return commit;
    }

    session.header.journal_sequence = session.manifest.generation;
    const auto persist = persist_header_and_superblock(session);
    if (!persist.is_ok()) {
        return persist;
    }
    return session.container.sync();
}

Result<void> VaultEngine::persist_header_and_superblock(UnlockedVault& session) {
    if (session.header_key.size() != 32) {
        return Result<void>::err(ErrorCode::PlatformError, "missing header key in session");
    }

    const auto confirm = key_manager_.compute_header_key_confirm(session.header_key.span());
    if (!confirm.is_ok()) {
        return Result<void>::err(confirm.error(), confirm.message());
    }
    session.header.header_key_confirm = confirm.value();

    if (session.hidden_volume) {
        const auto hidden = container::seal_header_sector(
            *crypto_,
            session.header,
            session.header_key.span(),
            session.superblock,
            container::HeaderSlotId::Hidden,
            container::HeaderSlotType::Hidden);
        if (!hidden.is_ok()) {
            return Result<void>::err(hidden.error(), hidden.message());
        }
        return session.container.write_header_sector(session.superblock.hidden_header_offset, hidden.value());
    }

    const auto primary = container::seal_header_sector(
        *crypto_,
        session.header,
        session.header_key.span(),
        session.superblock,
        container::HeaderSlotId::PrimaryOuter,
        container::HeaderSlotType::Outer);
    if (!primary.is_ok()) {
        return Result<void>::err(primary.error(), primary.message());
    }

    const auto backup = container::seal_header_sector(
        *crypto_,
        session.header,
        session.header_key.span(),
        session.superblock,
        container::HeaderSlotId::BackupOuter,
        container::HeaderSlotType::Outer);
    if (!backup.is_ok()) {
        return Result<void>::err(backup.error(), backup.message());
    }

    const auto write_backup = session.container.write_header_sector(
        session.superblock.backup_header_offset,
        backup.value());
    if (!write_backup.is_ok()) {
        return write_backup;
    }

    const auto write_primary = session.container.write_header_sector(
        session.superblock.primary_header_offset,
        primary.value());
    if (!write_primary.is_ok()) {
        return write_primary;
    }

    if (session.header.keyfile_required != 0) {
        session.superblock.flags |= container::SUPERBLOCK_FLAG_KEYFILE_REQUIRED;
    } else {
        session.superblock.flags &= ~container::SUPERBLOCK_FLAG_KEYFILE_REQUIRED;
    }
    return session.container.write_superblock_both_no_sync(session.superblock);
}

Result<void> VaultEngine::create_new(
    const std::filesystem::path& path,
    const CredentialMaterial& cred,
    const CreateVaultOptions& options) {
    if (!crypto_) {
        return Result<void>::err(ErrorCode::PlatformError, "crypto provider unavailable");
    }

    container::CreateLayoutParams layout_params {};
    layout_params.container_size = options.container_size;
    layout_params.quick_format = options.quick_format;
    const auto layout = container::compute_layout(layout_params);
    if (!layout.is_ok()) {
        return Result<void>::err(layout.error(), layout.message());
    }
    auto container = container::ContainerFileStore::create(path, layout.value().container_size);
    if (!container.is_ok()) {
        return Result<void>::err(container.error(), container.message());
    }

    container::Superblock sb {};
    std::memcpy(sb.magic.data(), container::SVLT_MAGIC, 4);
    sb.flags = container::SUPERBLOCK_FLAG_CREATED;
    if (options.quick_format) {
        sb.flags |= container::SUPERBLOCK_FLAG_QUICK_FORMAT;
    }
    if (!cred.keyfiles.empty()) {
        sb.flags |= container::SUPERBLOCK_FLAG_KEYFILE_REQUIRED;
    }
    sb.container_size = layout.value().container_size;
    sb.journal_offset = layout.value().journal_offset;
    sb.journal_size = layout.value().journal_size;
    sb.manifest_offset = layout.value().manifest_offset;
    sb.manifest_size = layout.value().manifest_size;
    sb.data_offset = layout.value().data_offset;
    sb.data_size = layout.value().data_size;

    const auto outer_salt = crypto_->random_bytes(sb.outer_salt);
    if (!outer_salt.is_ok()) {
        return Result<void>::err(outer_salt.error(), outer_salt.message());
    }
    const auto hidden_salt = crypto_->random_bytes(sb.hidden_salt);
    if (!hidden_salt.is_ok()) {
        return Result<void>::err(hidden_salt.error(), hidden_salt.message());
    }

    const auto kdf_secret = derive_kdf_secret(cred);
    if (!kdf_secret.is_ok()) {
        return Result<void>::err(kdf_secret.error(), kdf_secret.message());
    }

    std::array<std::uint8_t, 64> kdf_out {};
    auto secret = kdf_secret.value();
    const auto kdf = crypto_->kdf_argon2id(secret, sb.outer_salt, options.kdf_params, kdf_out);
    crypto_->secure_zero(secret);
    if (!kdf.is_ok()) {
        crypto_->secure_zero(kdf_out);
        return Result<void>::err(kdf.error(), kdf.message());
    }

    crypto::SecureBuffer header_key;
    header_key.resize(32);
    std::memcpy(header_key.data(), kdf_out.data(), 32);
    crypto_->secure_zero(kdf_out);

    const auto confirm = key_manager_.compute_header_key_confirm(header_key.span());
    if (!confirm.is_ok()) {
        return Result<void>::err(confirm.error(), confirm.message());
    }

    container::HeaderPayload payload {};
    payload.slot_type = static_cast<std::uint8_t>(container::HeaderSlotType::Outer);
    const auto vmk_random = crypto_->random_bytes(payload.volume_master_key);
    if (!vmk_random.is_ok()) {
        return Result<void>::err(vmk_random.error(), vmk_random.message());
    }
    payload.header_key_confirm = confirm.value();
    payload.manifest_offset = sb.manifest_offset;
    payload.manifest_size = sb.manifest_size;
    payload.journal_offset = sb.journal_offset;
    payload.journal_size = sb.journal_size;
    payload.data_offset = sb.data_offset;
    payload.data_size = sb.data_size;
    payload.chunk_size = options.chunk_size == 0 ? container::SVLT_DEFAULT_CHUNK_SIZE : options.chunk_size;
    encode_kdf_params_blob(options.kdf_params, payload.kdf_params_blob);
    store_outer_kdf_hint(sb, options.kdf_params);
    payload.manifest_generation = 1;
    payload.keyfile_required = cred.keyfiles.empty() ? 0 : 1;
    const std::uint64_t default_margin = std::max<std::uint64_t>(134217728ULL, sb.data_size * 5 / 100);
    payload.protect_margin_bytes = std::min(default_margin, sb.data_size / 2);

    const auto manifest = create_empty_manifest(sb.data_size, payload.chunk_size);
    if (!manifest.is_ok()) {
        return Result<void>::err(manifest.error(), manifest.message());
    }
    payload.manifest_root_hash = manifest.value().root_hash;

    const auto primary = container::seal_header_sector(
        *crypto_,
        payload,
        header_key.span(),
        sb,
        container::HeaderSlotId::PrimaryOuter,
        container::HeaderSlotType::Outer);
    if (!primary.is_ok()) {
        return Result<void>::err(primary.error(), primary.message());
    }

    const auto backup = container::seal_header_sector(
        *crypto_,
        payload,
        header_key.span(),
        sb,
        container::HeaderSlotId::BackupOuter,
        container::HeaderSlotType::Outer);
    if (!backup.is_ok()) {
        return Result<void>::err(backup.error(), backup.message());
    }
    container::EncryptedHeaderSector hidden_sector {};
    const auto hidden_nonce = crypto_->random_bytes(hidden_sector.header_nonce);
    if (!hidden_nonce.is_ok()) {
        return Result<void>::err(hidden_nonce.error(), hidden_nonce.message());
    }

    const auto write_sb = container.value().write_superblock_both(sb);
    if (!write_sb.is_ok()) {
        return write_sb;
    }

    const auto write_primary = container.value().write_header_sector(sb.primary_header_offset, primary.value());
    if (!write_primary.is_ok()) {
        return write_primary;
    }
    const auto write_backup = container.value().write_header_sector(sb.backup_header_offset, backup.value());
    if (!write_backup.is_ok()) {
        return write_backup;
    }
    const auto write_hidden = container.value().write_header_sector(sb.hidden_header_offset, hidden_sector);
    if (!write_hidden.is_ok()) {
        return write_hidden;
    }

    const auto pages = manifest_to_pages(manifest.value());
    if (!pages.is_ok()) {
        return Result<void>::err(pages.error(), pages.message());
    }

    for (std::size_t page_index = 0; page_index < 2; ++page_index) {
        const ManifestPage& page = page_index == 0 ? pages.value().first : pages.value().second;
        std::vector<std::uint8_t> slot(kManifestPageSlotSize);
        const auto enc = encrypt_manifest_page(
            *crypto_,
            payload.volume_master_key,
            page,
            Span<std::uint8_t>(slot.data(), kManifestMaxPagePlaintext),
            Span<std::uint8_t>(slot.data() + kManifestMaxPagePlaintext, crypto::kAeadTagSize));
        if (!enc.is_ok()) {
            return enc;
        }

        const std::uint64_t offset = sb.manifest_offset + page_index * kManifestPageSlotSize;
        const auto write_page = container.value().write_region(offset, as_bytes(slot));
        if (!write_page.is_ok()) {
            return write_page;
        }
    }

    recovery::Journal journal(
        container.value(),
        *crypto_,
        payload.volume_master_key,
        sb.journal_offset,
        sb.journal_size);
    const auto journal_init = journal.initialize();
    if (!journal_init.is_ok()) {
        return journal_init;
    }

    return container.value().sync();
}

Result<UnlockedVault> VaultEngine::open(
    const std::filesystem::path& path,
    const CredentialMaterial& cred,
    const OpenVaultOptions& options) {
    if (!crypto_) {
        return Result<UnlockedVault>::err(ErrorCode::PlatformError, "crypto provider unavailable");
    }

    auto container = container::ContainerFileStore::open(path);
    if (!container.is_ok()) {
        return Result<UnlockedVault>::err(container.error(), container.message());
    }

    const auto primary_sb = container.value().read_superblock_primary();
    if (!primary_sb.is_ok()) {
        return Result<UnlockedVault>::err(primary_sb.error(), primary_sb.message());
    }

    const auto tail_sb = container.value().read_superblock_tail();
    if (!tail_sb.is_ok()) {
        return Result<UnlockedVault>::err(tail_sb.error(), tail_sb.message());
    }
    if (primary_sb.value().crc32 != tail_sb.value().crc32) {
        return Result<UnlockedVault>::err(ErrorCode::CorruptContainer, "superblock copies differ");
    }

    if (options.intent != HeaderSlotIntent::Hidden
        && (primary_sb.value().flags & container::SUPERBLOCK_FLAG_KEYFILE_REQUIRED) != 0
        && cred.keyfiles.empty()) {
        return Result<UnlockedVault>::err(ErrorCode::WrongCredentials, "keyfile required");
    }

    const crypto::KdfParams kdf_params = options.kdf_params_override
        ? options.kdf_params
        : outer_kdf_params_from_superblock(primary_sb.value());
    const auto derived = key_manager_.derive_from_credentials(
        cred,
        primary_sb.value(),
        options.intent,
        kdf_params);
    if (!derived.is_ok()) {
        return Result<UnlockedVault>::err(derived.error(), derived.message());
    }

    container::HeaderPayload payload {};
    if (options.intent == HeaderSlotIntent::Hidden) {
        const auto hidden = container.value().read_header_sector(primary_sb.value().hidden_header_offset);
        if (!hidden.is_ok()) {
            return Result<UnlockedVault>::err(hidden.error(), hidden.message());
        }
        const auto opened = key_manager_.open_header(
            hidden.value(),
            derived.value(),
            primary_sb.value(),
            container::HeaderSlotId::Hidden,
            container::HeaderSlotType::Hidden);
        if (!opened.is_ok()) {
            return Result<UnlockedVault>::err(ErrorCode::WrongCredentials, "wrong credentials");
        }
        payload = opened.value();
    } else {
        const auto primary = container.value().read_header_sector(primary_sb.value().primary_header_offset);
        Result<container::HeaderPayload> opened = Result<container::HeaderPayload>::err(ErrorCode::WrongCredentials, "");
        if (primary.is_ok()) {
            opened = key_manager_.open_header(
                primary.value(),
                derived.value(),
                primary_sb.value(),
                container::HeaderSlotId::PrimaryOuter,
                container::HeaderSlotType::Outer);
        }
        if (!opened.is_ok()) {
            const auto backup = container.value().read_header_sector(primary_sb.value().backup_header_offset);
            if (!backup.is_ok()) {
                return Result<UnlockedVault>::err(ErrorCode::WrongCredentials, "wrong credentials");
            }
            opened = key_manager_.open_header(
                backup.value(),
                derived.value(),
                primary_sb.value(),
                container::HeaderSlotId::BackupOuter,
                container::HeaderSlotType::Outer);
        }
        if (!opened.is_ok()) {
            return Result<UnlockedVault>::err(ErrorCode::WrongCredentials, "wrong credentials");
        }
        payload = opened.value();
    }

    crypto::SecureBuffer vmk;
    vmk.resize(32);
    std::memcpy(vmk.data(), payload.volume_master_key.data(), 32);

    const auto manifest = read_manifest(container.value(), payload, vmk.span());
    if (!manifest.is_ok()) {
        return Result<UnlockedVault>::err(manifest.error(), manifest.message());
    }

    if (manifest.value().root_hash != payload.manifest_root_hash) {
        return Result<UnlockedVault>::err(ErrorCode::CorruptContainer, "manifest_root_hash mismatch");
    }

    UnlockedVault session {};
    session.path = path;
    session.container = std::move(container.value());
    session.superblock = primary_sb.value();
    session.header = payload;
    session.vmk = std::move(vmk);
    session.manifest = manifest.value();
    session.hidden_volume = options.intent == HeaderSlotIntent::Hidden;
    session.protect_hidden = options.protect_hidden;
    session.read_only = options.read_only;
    session.header_key.resize(32);
    std::memcpy(session.header_key.data(), derived.value().header_key.data(), 32);
    update_hidden_configured(session);

    {
        recovery::Journal journal(
            session.container,
            *crypto_,
            session.vmk.span(),
            session_journal_offset(session),
            session_journal_size(session));
        container::Superblock* dirty_tracker =
            session.hidden_volume ? nullptr : &session.superblock;
        const auto replay = journal.replay_if_dirty(dirty_tracker, session.hidden_volume);
        if (!replay.is_ok()) {
            return Result<UnlockedVault>::err(replay.error(), replay.message());
        }
    }

    return Result<UnlockedVault>::ok(std::move(session));
}

Result<void> VaultEngine::lock(const std::filesystem::path& path) {
    return SessionStore::instance().remove(path);
}

Result<std::uint64_t> VaultEngine::alloc_inode_id(const ManifestState& manifest) const {
    if (manifest.inodes.empty()) {
        return Result<std::uint64_t>::ok(2);
    }
    return Result<std::uint64_t>::ok(
        std::max_element(
            manifest.inodes.begin(),
            manifest.inodes.end(),
            [](const Inode& a, const Inode& b) { return a.inode_id < b.inode_id; })
            ->inode_id
        + 1);
}

Result<std::uint64_t> VaultEngine::resolve_parent_id(
    const ManifestState& manifest,
    const std::filesystem::path& vault_path) const {
    const auto normalized = normalize_vault_path(vault_path);
    if (normalized == "/" || normalized.empty()) {
        return Result<std::uint64_t>::ok(0);
    }

    const auto parent_path = normalized.parent_path();
    const auto name = normalized.filename().generic_string();

    const auto parent_id = resolve_parent_id(manifest, parent_path);
    if (!parent_id.is_ok()) {
        return parent_id;
    }

    const auto it = find_inode(manifest.inodes, parent_id.value(), name);
    if (it == manifest.inodes.end()) {
        return Result<std::uint64_t>::err(ErrorCode::InvalidArgument, "path not found");
    }
    if (it->entry_type != container::InodeEntryType::Directory) {
        return Result<std::uint64_t>::err(ErrorCode::InvalidArgument, "path component is not a directory");
    }
    return Result<std::uint64_t>::ok(it->inode_id);
}

Result<std::uint64_t> VaultEngine::resolve_parent_id(
    ManifestState& manifest,
    const std::filesystem::path& vault_path,
    const bool create_missing) const {
    const auto normalized = normalize_vault_path(vault_path);
    if (normalized == "/" || normalized.empty()) {
        return Result<std::uint64_t>::ok(0);
    }

    const auto parent_path = normalized.parent_path();
    const auto name = normalized.filename().generic_string();

    const auto parent_id = resolve_parent_id(manifest, parent_path, create_missing);
    if (!parent_id.is_ok()) {
        return parent_id;
    }

    const auto it = find_inode(manifest.inodes, parent_id.value(), name);
    if (it != manifest.inodes.end()) {
        if (it->entry_type != container::InodeEntryType::Directory) {
            return Result<std::uint64_t>::err(ErrorCode::InvalidArgument, "path component is not a directory");
        }
        return Result<std::uint64_t>::ok(it->inode_id);
    }

    if (!create_missing) {
        return Result<std::uint64_t>::err(ErrorCode::InvalidArgument, "path not found");
    }

    if (manifest.inodes.size() >= container::SVLT_MAX_INODES_V1) {
        return Result<std::uint64_t>::err(ErrorCode::ManifestFull, "inode table full");
    }

    Inode dir {};
    dir.entry_type = container::InodeEntryType::Directory;
    dir.name_utf8 = name;
    dir.parent_id = parent_id.value();
    const auto inode_id = alloc_inode_id(manifest);
    if (!inode_id.is_ok()) {
        return inode_id;
    }
    dir.inode_id = inode_id.value();
    dir.mtime_unix = unix_now();
    manifest.inodes.push_back(dir);
    std::sort(manifest.inodes.begin(), manifest.inodes.end(), inode_less);
    return Result<std::uint64_t>::ok(dir.inode_id);
}

Result<std::uint64_t> VaultEngine::allocate_chunks(
    ManifestState& manifest,
    const std::uint32_t count,
    const std::uint64_t data_size,
    const std::uint32_t chunk_size) const {
    if (count == 0) {
        return Result<std::uint64_t>::ok(0);
    }

    const std::uint64_t total_chunks = chunk_count_for_data(data_size, chunk_size);
    if (count > total_chunks) {
        return Result<std::uint64_t>::err(ErrorCode::IoError, "no free chunks available");
    }

    for (std::uint64_t start = 0; start + count <= total_chunks; ++start) {
        bool run_free = true;
        for (std::uint32_t offset = 0; offset < count; ++offset) {
            if (get_bitmap_state(manifest.bitmap, start + offset) != 0) {
                run_free = false;
                break;
            }
        }
        if (!run_free) {
            continue;
        }

        for (std::uint32_t offset = 0; offset < count; ++offset) {
            set_bitmap_state(manifest.bitmap, start + offset, 1);
        }
        return Result<std::uint64_t>::ok(start);
    }

    return Result<std::uint64_t>::err(ErrorCode::IoError, "no free chunks available");
}

Result<void> VaultEngine::put(
    UnlockedVault& session,
    const std::filesystem::path& vault_path,
    const Span<const std::uint8_t> data,
    const bool force) {
    if (session.read_only) {
        return Result<void>::err(ErrorCode::InvalidArgument, "vault session is read-only");
    }

    const auto normalized = normalize_vault_path(vault_path);
    const auto parent_path = normalized.parent_path();
    const auto name = normalized.filename().generic_string();
    if (name.empty()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "invalid vault path");
    }

    const auto parent_id = resolve_parent_id(session.manifest, parent_path, true);
    if (!parent_id.is_ok()) {
        return Result<void>::err(parent_id.error(), parent_id.message());
    }

    auto it = find_inode(session.manifest.inodes, parent_id.value(), name);
    const bool exists = it != session.manifest.inodes.end();

    if (exists && !force) {
        return Result<void>::err(ErrorCode::InvalidArgument, "file already exists");
    }

    ChunkStore chunks(session.container, *crypto_, session.header.data_offset, session.header.chunk_size);
    const std::uint32_t needed_chunks = data.empty()
        ? 0
        : static_cast<std::uint32_t>((data.size() + session.header.chunk_size - 1) / session.header.chunk_size);

    Inode inode {};
    Inode previous_inode {};
    if (exists) {
        inode = *it;
        previous_inode = *it;
    } else {
        if (session.manifest.inodes.size() >= container::SVLT_MAX_INODES_V1) {
            return Result<void>::err(ErrorCode::ManifestFull, "inode table full");
        }
        inode.inode_version = 1;
        inode.entry_type = container::InodeEntryType::File;
        inode.name_utf8 = name;
        inode.parent_id = parent_id.value();
        inode.inode_id = session.manifest.inodes.empty()
            ? 2
            : std::max_element(
                  session.manifest.inodes.begin(),
                  session.manifest.inodes.end(),
                  [](const Inode& a, const Inode& b) { return a.inode_id < b.inode_id; })->inode_id
                + 1;
        const auto nonce = crypto_->random_bytes(inode.base_nonce);
        if (!nonce.is_ok()) {
            return Result<void>::err(nonce.error(), nonce.message());
        }
    }

    std::uint64_t first_chunk_id = 0;
    if (needed_chunks > 0) {
        const auto first_chunk = allocate_chunks(
            session.manifest,
            needed_chunks,
            session.header.data_size,
            session.header.chunk_size);
        if (!first_chunk.is_ok()) {
            return Result<void>::err(first_chunk.error(), first_chunk.message());
        }
        first_chunk_id = first_chunk.value();

        const auto write_end = session.header.data_offset
            + (first_chunk_id + needed_chunks)
                * (static_cast<std::uint64_t>(session.header.chunk_size) + crypto::kAeadTagSize);
        const auto allowed = check_write_allowed(session, write_end);
        if (!allowed.is_ok()) {
            for (std::uint32_t index = 0; index < needed_chunks; ++index) {
                set_bitmap_state(session.manifest.bitmap, first_chunk_id + index, 0);
            }
            return allowed;
        }
    }

    const std::uint64_t pending_generation = session.manifest.generation + 1;
    const auto written = chunks.write_file(
        inode,
        session.vmk.span(),
        pending_generation,
        data,
        first_chunk_id);
    if (!written.is_ok()) {
        for (std::uint32_t index = 0; index < needed_chunks; ++index) {
            set_bitmap_state(session.manifest.bitmap, first_chunk_id + index, 0);
        }
        return Result<void>::err(written.error(), written.message());
    }

    if (exists && previous_inode.chunk_count > 0) {
        const bool same_allocation = previous_inode.first_chunk_id == first_chunk_id
            && previous_inode.chunk_count == written.value().chunk_count;
        if (!same_allocation) {
            free_file_chunks(session.manifest, previous_inode);
        } else if (written.value().chunk_count < previous_inode.chunk_count) {
            for (std::uint32_t index = written.value().chunk_count; index < previous_inode.chunk_count; ++index) {
                set_bitmap_state(
                    session.manifest.bitmap,
                    previous_inode.first_chunk_id + index,
                    0);
            }
        }
    }

    inode.size = data.size();
    inode.mtime_unix = unix_now();
    inode.chunk_count = written.value().chunk_count;
    inode.first_chunk_id = written.value().first_chunk_id;
    inode.data_generation = pending_generation;

    if (exists) {
        *it = inode;
    } else {
        session.manifest.inodes.push_back(inode);
        std::sort(session.manifest.inodes.begin(), session.manifest.inodes.end(), inode_less);
    }

    return write_manifest(session);
}

Result<std::vector<std::uint8_t>> VaultEngine::get(
    const UnlockedVault& session,
    const std::filesystem::path& vault_path) const {
    const auto normalized = normalize_vault_path(vault_path);
    const auto parent_path = normalized.parent_path();
    const auto name = normalized.filename().generic_string();

    const auto parent_id = resolve_parent_id(session.manifest, parent_path);
    if (!parent_id.is_ok()) {
        return Result<std::vector<std::uint8_t>>::err(parent_id.error(), parent_id.message());
    }

    const auto it = find_inode(session.manifest.inodes, parent_id.value(), name);
    if (it == session.manifest.inodes.end() || it->entry_type != container::InodeEntryType::File) {
        return Result<std::vector<std::uint8_t>>::err(ErrorCode::InvalidArgument, "file not found");
    }

    ChunkStore chunks(
        const_cast<container::ContainerFileStore&>(session.container),
        *crypto_,
        session.header.data_offset,
        session.header.chunk_size);
    const auto generation = it->data_generation != 0 ? it->data_generation : session.manifest.generation;
    return chunks.read_file(*it, session.vmk.span(), generation);
}

Result<std::vector<VaultEntry>> VaultEngine::list(
    const UnlockedVault& session,
    const std::filesystem::path& vault_dir) const {
    const auto parent_id = resolve_parent_id(session.manifest, vault_dir);
    if (!parent_id.is_ok()) {
        return Result<std::vector<VaultEntry>>::err(parent_id.error(), parent_id.message());
    }

    std::vector<VaultEntry> entries;
    for (const auto& inode : session.manifest.inodes) {
        if (inode.parent_id != parent_id.value()) {
            continue;
        }
        if (inode.entry_type == container::InodeEntryType::HiddenReservation) {
            continue;
        }
        if (inode.name_utf8.empty()) {
            continue;
        }
        VaultEntry entry {};
        entry.name = inode.name_utf8;
        entry.mtime = inode.mtime_unix;
        if (inode.entry_type == container::InodeEntryType::Directory) {
            entry.type = "dir";
        } else {
            entry.type = "file";
            entry.size = inode.size;
        }
        entries.push_back(std::move(entry));
    }

    std::sort(entries.begin(), entries.end(), [](const VaultEntry& a, const VaultEntry& b) {
        return a.name < b.name;
    });
    return Result<std::vector<VaultEntry>>::ok(std::move(entries));
}

namespace {

void collect_descendants(const ManifestState& manifest, const std::uint64_t inode_id, std::vector<std::uint64_t>& out) {
    for (const auto& inode : manifest.inodes) {
        if (inode.parent_id == inode_id) {
            out.push_back(inode.inode_id);
            if (inode.entry_type == container::InodeEntryType::Directory) {
                collect_descendants(manifest, inode.inode_id, out);
            }
        }
    }
}

} // namespace

Result<void> VaultEngine::remove(UnlockedVault& session, const std::filesystem::path& vault_path) {
    const auto normalized = normalize_vault_path(vault_path);
    const auto parent_path = normalized.parent_path();
    const auto name = normalized.filename().generic_string();

    const auto parent_id = resolve_parent_id(session.manifest, parent_path);
    if (!parent_id.is_ok()) {
        return Result<void>::err(parent_id.error(), parent_id.message());
    }

    const auto it = find_inode(session.manifest.inodes, parent_id.value(), name);
    if (it == session.manifest.inodes.end()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "path not found");
    }

    const std::uint64_t target_id = it->inode_id;
    std::vector<std::uint64_t> to_remove;
    if (it->entry_type == container::InodeEntryType::Directory) {
        collect_descendants(session.manifest, target_id, to_remove);
    } else {
        free_file_chunks(session.manifest, *it);
    }

    for (const std::uint64_t inode_id : to_remove) {
        const auto child_it = std::find_if(
            session.manifest.inodes.begin(),
            session.manifest.inodes.end(),
            [inode_id](const Inode& inode) { return inode.inode_id == inode_id; });
        if (child_it == session.manifest.inodes.end()) {
            continue;
        }
        if (child_it->entry_type == container::InodeEntryType::File) {
            free_file_chunks(session.manifest, *child_it);
        }
    }

    to_remove.push_back(target_id);
    std::sort(to_remove.begin(), to_remove.end());
    to_remove.erase(std::unique(to_remove.begin(), to_remove.end()), to_remove.end());

    for (const std::uint64_t inode_id : to_remove) {
        const auto erase_it = std::find_if(
            session.manifest.inodes.begin(),
            session.manifest.inodes.end(),
            [inode_id](const Inode& inode) { return inode.inode_id == inode_id; });
        if (erase_it != session.manifest.inodes.end()) {
            session.manifest.inodes.erase(erase_it);
        }
    }

    return write_manifest(session, false);
}

Result<void> VaultEngine::rename(
    UnlockedVault& session,
    const std::filesystem::path& from_path,
    const std::filesystem::path& to_path) {
    const auto from = normalize_vault_path(from_path);
    const auto to = normalize_vault_path(to_path);

    const auto from_parent = resolve_parent_id(session.manifest, from.parent_path());
    if (!from_parent.is_ok()) {
        return Result<void>::err(from_parent.error(), from_parent.message());
    }
    const auto to_parent = resolve_parent_id(session.manifest, to.parent_path());
    if (!to_parent.is_ok()) {
        return Result<void>::err(to_parent.error(), to_parent.message());
    }

    const auto from_name = from.filename().generic_string();
    const auto to_name = to.filename().generic_string();
    auto it = find_inode(session.manifest.inodes, from_parent.value(), from_name);
    if (it == session.manifest.inodes.end()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "source path not found");
    }
    if (find_inode(session.manifest.inodes, to_parent.value(), to_name) != session.manifest.inodes.end()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "destination already exists");
    }

    it->name_utf8 = to_name;
    it->parent_id = to_parent.value();
    std::sort(session.manifest.inodes.begin(), session.manifest.inodes.end(), inode_less);
    return write_manifest(session, false);
}

Result<void> VaultEngine::mkdir(UnlockedVault& session, const std::filesystem::path& vault_dir) {
    const auto normalized = normalize_vault_path(vault_dir);
    const auto parent_path = normalized.parent_path();
    const auto name = normalized.filename().generic_string();
    if (name.empty()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "invalid directory path");
    }

    const auto parent_id = resolve_parent_id(session.manifest, parent_path, true);
    if (!parent_id.is_ok()) {
        return Result<void>::err(parent_id.error(), parent_id.message());
    }
    if (find_inode(session.manifest.inodes, parent_id.value(), name) != session.manifest.inodes.end()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "directory already exists");
    }
    if (session.manifest.inodes.size() >= container::SVLT_MAX_INODES_V1) {
        return Result<void>::err(ErrorCode::ManifestFull, "inode table full");
    }

    Inode dir {};
    dir.entry_type = container::InodeEntryType::Directory;
    dir.name_utf8 = name;
    dir.parent_id = parent_id.value();
    const auto inode_id = alloc_inode_id(session.manifest);
    if (!inode_id.is_ok()) {
        return Result<void>::err(inode_id.error(), inode_id.message());
    }
    dir.inode_id = inode_id.value();
    dir.mtime_unix = unix_now();
    session.manifest.inodes.push_back(dir);
    std::sort(session.manifest.inodes.begin(), session.manifest.inodes.end(), inode_less);
    return write_manifest(session, false);
}

Result<void> VaultEngine::commit_manifest(UnlockedVault& session) {
    return write_manifest(session);
}

Result<void> VaultEngine::change_password(
    const std::filesystem::path& path,
    const CredentialMaterial& old_cred,
    const CredentialMaterial& new_cred) {
    OpenVaultOptions open_opts {};
    auto session = open(path, old_cred, open_opts);
    if (!session.is_ok()) {
        return Result<void>::err(session.error(), session.message());
    }

    const auto kdf_params = kdf_params_from_blob(session.value().header.kdf_params_blob);
    const auto derived = key_manager_.derive_from_credentials(
        new_cred,
        session.value().superblock,
        HeaderSlotIntent::Outer,
        kdf_params);
    if (!derived.is_ok()) {
        return Result<void>::err(derived.error(), derived.message());
    }

    session.value().header.keyfile_required = new_cred.keyfiles.empty() ? 0 : 1;
    std::memcpy(session.value().header_key.data(), derived.value().header_key.data(), 32);
    const auto persist = persist_header_and_superblock(session.value());
    if (!persist.is_ok()) {
        return persist;
    }

    const auto synced = session.value().container.sync();
    (void)lock(path);
    return synced;
}

Result<void> VaultEngine::add_keyfile(
    const std::filesystem::path& path,
    const CredentialMaterial& old_cred,
    const keyfile::KeyfileEntry& new_keyfile) {
    OpenVaultOptions open_opts {};
    auto session = open(path, old_cred, open_opts);
    if (!session.is_ok()) {
        return Result<void>::err(session.error(), session.message());
    }

    CredentialMaterial new_cred = old_cred;
    new_cred.keyfiles.push_back(new_keyfile);
    std::sort(new_cred.keyfiles.begin(), new_cred.keyfiles.end(), [](const keyfile::KeyfileEntry& a, const keyfile::KeyfileEntry& b) {
        return a.path < b.path;
    });

    const auto kdf_params = kdf_params_from_blob(session.value().header.kdf_params_blob);
    const auto derived = key_manager_.derive_from_credentials(
        new_cred,
        session.value().superblock,
        HeaderSlotIntent::Outer,
        kdf_params);
    if (!derived.is_ok()) {
        return Result<void>::err(derived.error(), derived.message());
    }

    session.value().header.keyfile_required = 1;
    std::memcpy(session.value().header_key.data(), derived.value().header_key.data(), 32);
    const auto persist = persist_header_and_superblock(session.value());
    if (!persist.is_ok()) {
        return persist;
    }

    const auto synced = session.value().container.sync();
    (void)lock(path);
    return synced;
}

Result<void> VaultEngine::remove_keyfile(
    const std::filesystem::path& path,
    const CredentialMaterial& old_cred,
    const keyfile::KeyfileHash& keyfile_hash) {
    OpenVaultOptions open_opts {};
    auto session = open(path, old_cred, open_opts);
    if (!session.is_ok()) {
        return Result<void>::err(session.error(), session.message());
    }

    CredentialMaterial new_cred = old_cred;
    const auto end = std::remove_if(
        new_cred.keyfiles.begin(),
        new_cred.keyfiles.end(),
        [&](const keyfile::KeyfileEntry& entry) {
            return entry.hash == keyfile_hash;
        });
    if (end == new_cred.keyfiles.end()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "keyfile not in vault credentials");
    }
    new_cred.keyfiles.erase(end, new_cred.keyfiles.end());

    const auto kdf_params = kdf_params_from_blob(session.value().header.kdf_params_blob);
    const auto derived = key_manager_.derive_from_credentials(
        new_cred,
        session.value().superblock,
        HeaderSlotIntent::Outer,
        kdf_params);
    if (!derived.is_ok()) {
        return Result<void>::err(derived.error(), derived.message());
    }

    session.value().header.keyfile_required = new_cred.keyfiles.empty() ? 0 : 1;
    std::memcpy(session.value().header_key.data(), derived.value().header_key.data(), 32);
    const auto persist = persist_header_and_superblock(session.value());
    if (!persist.is_ok()) {
        return persist;
    }

    const auto synced = session.value().container.sync();
    (void)lock(path);
    return synced;
}

} // namespace sv::vault