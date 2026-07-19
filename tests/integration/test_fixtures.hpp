#pragma once

#include <sv/container/container_file.hpp>
#include <sv/container/types.hpp>
#include <sv/crypto/kdf_engine.hpp>
#include <sv/vault/credential_material.hpp>
#include <sv/vault/hidden_volume.hpp>
#include <sv/vault/vault_engine.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>

namespace sv::test {

inline std::filesystem::path temp_vault_path(const char* prefix) {
    static std::atomic<std::uint64_t> counter {0};
    const auto stamp = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto id = counter.fetch_add(1, std::memory_order_relaxed);
    return std::filesystem::path("C:/Temp")
        / (std::string(prefix) + "_" + std::to_string(stamp) + "_" + std::to_string(id) + ".svlt");
}

inline void cleanup_vault(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

inline vault::CredentialMaterial outer_credentials() {
    vault::CredentialMaterial cred {};
    cred.password_utf8 = "outer-integration-password-12";
    return cred;
}

inline vault::CredentialMaterial hidden_credentials() {
    vault::CredentialMaterial cred {};
    cred.password_utf8 = "hidden-integration-password-9";
    return cred;
}

inline vault::CreateVaultOptions hidden_test_create_options() {
    vault::CreateVaultOptions options {};
    options.container_size = 128ULL * 1024 * 1024;
    options.chunk_size = 64 * 1024;
    options.kdf_params.m_cost_kib = 4096;
    options.kdf_params.t_cost = 1;
    options.kdf_params.p_cost = 1;
    return options;
}

inline vault::CreateVaultOptions file_ops_create_options() {
    vault::CreateVaultOptions options {};
    options.container_size = 16ULL * 1024 * 1024;
    options.chunk_size = 64 * 1024;
    options.kdf_params.m_cost_kib = 4096;
    options.kdf_params.t_cost = 1;
    options.kdf_params.p_cost = 1;
    return options;
}


inline vault::CreateVaultOptions pba_lab_create_options() {
    vault::CreateVaultOptions options {};
    options.container_size = 4ULL * 1024 * 1024;
    options.chunk_size = 64 * 1024;
    options.kdf_params.m_cost_kib = 64;
    options.kdf_params.t_cost = 1;
    options.kdf_params.p_cost = 1;
    return options;
}

inline bool slow_tests_enabled() {
#if defined(_MSC_VER)
    char* env = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&env, &len, "SV_RUN_SLOW_TESTS") != 0 || env == nullptr) {
        return false;
    }
    const bool enabled = env[0] == '1' && env[1] == '\0';
    free(env);
    return enabled;
#else
    const char* env = std::getenv("SV_RUN_SLOW_TESTS");
    return env != nullptr && env[0] == '1' && env[1] == '\0';
#endif
}

inline vault::CreateVaultOptions gib_file_create_options() {
    vault::CreateVaultOptions options {};
    options.container_size = 3ULL * 1024 * 1024 * 1024;
    options.chunk_size = 1024 * 1024;
    options.kdf_params.m_cost_kib = 4096;
    options.kdf_params.t_cost = 1;
    options.kdf_params.p_cost = 1;
    return options;
}

inline bool integration_release_only() {
#if !defined(NDEBUG)
    return true;
#else
    return false;
#endif
}

inline void release_session(sv::vault::UnlockedVault& session) {
    session = sv::vault::UnlockedVault {};
}

inline std::uint64_t hidden_size_min() {
    return 32ULL * 1024 * 1024;
}

inline sv::Result<void> create_hidden_test_vault(
    const std::filesystem::path& path,
    sv::vault::VaultEngine& engine) {
    const auto created = engine.create_new(path, outer_credentials(), hidden_test_create_options());
    if (!created.is_ok()) {
        return created;
    }

    sv::vault::OpenVaultOptions outer_opts {};
    auto outer = engine.open(path, outer_credentials(), outer_opts);
    if (!outer.is_ok()) {
        return sv::Result<void>::err(outer.error(), outer.message());
    }

    const auto hidden = sv::vault::create_hidden_volume(
        engine,
        outer.value(),
        hidden_credentials(),
        hidden_size_min());
    sv::test::release_session(outer.value());
    return hidden;
}

inline sv::Result<std::vector<std::uint8_t>> read_header_region(
    const std::filesystem::path& path,
    std::uint64_t offset) {
    auto container = sv::container::ContainerFileStore::open(path);
    if (!container.is_ok()) {
        return sv::Result<std::vector<std::uint8_t>>::err(container.error(), container.message());
    }
    return container.value().read_region(offset, sv::container::SVLT_HEADER_SIZE);
}

inline sv::Result<std::vector<std::uint8_t>> read_tail_superblock_region(const std::filesystem::path& path) {
    auto container = sv::container::ContainerFileStore::open(path);
    if (!container.is_ok()) {
        return sv::Result<std::vector<std::uint8_t>>::err(container.error(), container.message());
    }
    const auto sb = container.value().read_superblock_tail();
    if (!sb.is_ok()) {
        return sv::Result<std::vector<std::uint8_t>>::err(sb.error(), sb.message());
    }
    const auto offset = sb.value().container_size - sv::container::SVLT_SUPERBLOCK_SIZE;
    return container.value().read_region(offset, sv::container::SVLT_SUPERBLOCK_SIZE);
}

inline sv::Result<void> corrupt_primary_outer_header(const std::filesystem::path& path) {
    auto container = sv::container::ContainerFileStore::open(path);
    if (!container.is_ok()) {
        return sv::Result<void>::err(container.error(), container.message());
    }

    const auto sb = container.value().read_superblock_primary();
    if (!sb.is_ok()) {
        return sv::Result<void>::err(sb.error(), sb.message());
    }

    std::vector<std::uint8_t> garbage(sv::container::SVLT_HEADER_SIZE, 0xFF);
    const auto write = container.value().write_region(sb.value().primary_header_offset, sv::as_bytes(garbage));
    if (!write.is_ok()) {
        return write;
    }
    return container.value().sync();
}

} 