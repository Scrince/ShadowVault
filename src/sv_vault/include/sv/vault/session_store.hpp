#pragma once

#include <sv/container/container_file.hpp>
#include <sv/container/types.hpp>
#include <sv/crypto/secure_buffer.hpp>
#include <sv/result.hpp>
#include <sv/vault/key_manager.hpp>
#include <sv/vault/manifest.hpp>

#include <filesystem>
#include <memory>
#include <optional>

namespace sv::vault {

struct UnlockedVault {
    std::filesystem::path path;
    container::ContainerFileStore container;
    container::Superblock superblock;
    container::HeaderPayload header;
    crypto::SecureBuffer vmk;
    crypto::SecureBuffer header_key;
    ManifestState manifest;
    bool hidden_volume = false;
    bool protect_hidden = false;
    bool hidden_configured = false;
    bool read_only = false;
};

class SessionStore {
public:
    [[nodiscard]] static SessionStore& instance() noexcept;

    [[nodiscard]] Result<UnlockedVault*> find(const std::filesystem::path& path);
    [[nodiscard]] Result<UnlockedVault*> find_any();
    [[nodiscard]] bool has_session(const std::filesystem::path& path) const noexcept;

    [[nodiscard]] Result<void> insert(UnlockedVault session, bool replace = false);
    [[nodiscard]] Result<void> remove(const std::filesystem::path& path);
    void clear() noexcept;

private:
    SessionStore() = default;

    std::vector<std::unique_ptr<UnlockedVault>> sessions_;
};

} // namespace sv::vault