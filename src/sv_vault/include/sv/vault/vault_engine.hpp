#pragma once

#include <sv/container/container_file.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/result.hpp>
#include <sv/vault/credential_material.hpp>
#include <sv/vault/key_manager.hpp>
#include <sv/vault/manifest.hpp>
#include <sv/vault/session_store.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace sv::vault {

struct CreateVaultOptions {
    std::uint64_t container_size = 0;
    bool quick_format = false;
    std::uint32_t chunk_size = container::SVLT_DEFAULT_CHUNK_SIZE;
    crypto::KdfParams kdf_params = default_kdf_params_desktop();
};

struct OpenVaultOptions {
    HeaderSlotIntent intent = HeaderSlotIntent::Outer;
    bool protect_hidden = false;
    bool read_only = false;
    bool kdf_params_override = false;
    crypto::KdfParams kdf_params = default_kdf_params_desktop();
};

struct VaultEntry {
    std::string name;
    std::string type;
    std::uint64_t size = 0;
    std::uint64_t mtime = 0;
};

class VaultEngine {
public:
    VaultEngine();

    [[nodiscard]] Result<void> create_new(
        const std::filesystem::path& path,
        const CredentialMaterial& cred,
        const CreateVaultOptions& options);

    [[nodiscard]] Result<UnlockedVault> open(
        const std::filesystem::path& path,
        const CredentialMaterial& cred,
        const OpenVaultOptions& options);

    [[nodiscard]] Result<void> lock(const std::filesystem::path& path);

    [[nodiscard]] Result<void> put(
        UnlockedVault& session,
        const std::filesystem::path& vault_path,
        Span<const std::uint8_t> data,
        bool force = false);

    [[nodiscard]] Result<std::vector<std::uint8_t>> get(
        const UnlockedVault& session,
        const std::filesystem::path& vault_path) const;

    [[nodiscard]] Result<std::vector<VaultEntry>> list(
        const UnlockedVault& session,
        const std::filesystem::path& vault_dir) const;

    [[nodiscard]] Result<void> remove(UnlockedVault& session, const std::filesystem::path& vault_path);
    [[nodiscard]] Result<void> rename(
        UnlockedVault& session,
        const std::filesystem::path& from_path,
        const std::filesystem::path& to_path);
    [[nodiscard]] Result<void> mkdir(UnlockedVault& session, const std::filesystem::path& vault_dir);

    [[nodiscard]] Result<void> change_password(
        const std::filesystem::path& path,
        const CredentialMaterial& old_cred,
        const CredentialMaterial& new_cred);

    [[nodiscard]] Result<void> add_keyfile(
        const std::filesystem::path& path,
        const CredentialMaterial& old_cred,
        const keyfile::KeyfileEntry& new_keyfile);

    [[nodiscard]] Result<void> remove_keyfile(
        const std::filesystem::path& path,
        const CredentialMaterial& old_cred,
        const keyfile::KeyfileHash& keyfile_hash);

    [[nodiscard]] Result<void> commit_manifest(UnlockedVault& session);

private:
    [[nodiscard]] Result<void> write_manifest(UnlockedVault& session, bool bump_generation = true);
    [[nodiscard]] Result<ManifestState> read_manifest(
        container::ContainerFileStore& container,
        const container::HeaderPayload& header,
        Span<const std::uint8_t> vmk) const;

    [[nodiscard]] Result<std::uint64_t> resolve_parent_id(
        const ManifestState& manifest,
        const std::filesystem::path& vault_path) const;

    [[nodiscard]] Result<std::uint64_t> resolve_parent_id(
        ManifestState& manifest,
        const std::filesystem::path& vault_path,
        bool create_missing) const;

    [[nodiscard]] Result<std::uint64_t> alloc_inode_id(const ManifestState& manifest) const;

    [[nodiscard]] Result<std::uint64_t> allocate_chunks(
        ManifestState& manifest,
        std::uint32_t count,
        std::uint64_t data_size,
        std::uint32_t chunk_size) const;

    [[nodiscard]] Result<void> persist_header_and_superblock(UnlockedVault& session);

    std::unique_ptr<crypto::CryptoProvider> crypto_;
    KeyManager key_manager_;
};

} 