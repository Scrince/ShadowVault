#include <sv/mount/mount_manager.hpp>
#include <sv/vault/session_store.hpp>
#include <sv/vault/vault_engine.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "test_fixtures.hpp"

#if defined(SV_HAVE_WINFSP) || defined(SV_HAVE_FUSE3)
#define SV_MOUNT_BACKEND_AVAILABLE 1
#endif

namespace {

std::filesystem::path temp_mount_dir(const char* prefix) {
    static std::atomic<std::uint64_t> counter {0};
    const auto id = counter.fetch_add(1, std::memory_order_relaxed);
    const auto dir = std::filesystem::path("C:/Temp")
        / (std::string(prefix) + "_" + std::to_string(id));
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::filesystem::path pick_mount_point() {
    // WinFsp FUSE is most reliable with a drive-letter mount on Windows.
    for (char letter = 'Y'; letter >= 'M'; --letter) {
        const auto candidate = std::filesystem::path(std::string(1, letter) + ":");
        std::error_code ec;
        if (!std::filesystem::exists(candidate, ec)) {
            return candidate;
        }
    }
    return temp_mount_dir("sv_mt_fallback");
}

void cleanup_mount_dir(const std::filesystem::path& dir) {
    if (dir.generic_string().size() <= 2 && dir.generic_string().back() == ':') {
        return;
    }
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

} // namespace

TEST_CASE("MT-01 mount roundtrip create file via OS", "[integration][mount][mt01]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Mount integration tests run in Release builds");
    }
#if !defined(SV_MOUNT_BACKEND_AVAILABLE)
    SKIP("FUSE/WinFsp mount backend not available in this build");
#endif

    const auto vault_path = sv::test::temp_vault_path("sv_mt01");
    (void)sv::mount::MountManager::instance().dismount_all();
    const auto mount_dir = pick_mount_point();
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(vault_path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions open_opts {};
    auto session = engine.open(vault_path, sv::test::outer_credentials(), open_opts);
    REQUIRE(session.is_ok());
    REQUIRE(sv::vault::SessionStore::instance().insert(std::move(session.value()), true).is_ok());

    sv::mount::MountOptions opts {};
    opts.mount_point = mount_dir;
    auto& manager = sv::mount::MountManager::instance();
    const auto mounted = manager.mount_vault(vault_path, opts);
    REQUIRE(mounted.is_ok());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    REQUIRE(std::filesystem::exists(mount_dir));

    const auto* unlocked = sv::vault::SessionStore::instance().find(vault_path).value();
    const std::string payload = "mounted hello";
    REQUIRE(engine.put(*const_cast<sv::vault::UnlockedVault*>(unlocked), "/hello.txt", sv::Span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(payload.data()),
        payload.size()), false).is_ok());

    const auto os_file = std::filesystem::path(mount_dir.generic_string() + "\\hello.txt");
    {
        HANDLE handle = CreateFileW(
            os_file.wstring().c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        REQUIRE(handle != INVALID_HANDLE_VALUE);
        char buffer[32] {};
        DWORD read = 0;
        REQUIRE(ReadFile(handle, buffer, sizeof(buffer) - 1, &read, nullptr));
        CloseHandle(handle);
        REQUIRE(std::string(buffer, read) == payload);
    }

    REQUIRE(manager.dismount(mounted.value()).is_ok());
    cleanup_mount_dir(mount_dir);
    sv::test::cleanup_vault(vault_path);
}

TEST_CASE("MT-07 OS write roundtrip via mount", "[integration][mount][mt07]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Mount integration tests run in Release builds");
    }
#if !defined(SV_MOUNT_BACKEND_AVAILABLE)
    SKIP("FUSE/WinFsp mount backend not available in this build");
#endif

    const auto vault_path = sv::test::temp_vault_path("sv_mt07");
    (void)sv::mount::MountManager::instance().dismount_all();
    const auto mount_dir = pick_mount_point();
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(vault_path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions open_opts {};
    auto session = engine.open(vault_path, sv::test::outer_credentials(), open_opts);
    REQUIRE(session.is_ok());
    REQUIRE(sv::vault::SessionStore::instance().insert(std::move(session.value()), true).is_ok());

    sv::mount::MountOptions opts {};
    opts.mount_point = mount_dir;
    auto& manager = sv::mount::MountManager::instance();
    const auto mounted = manager.mount_vault(vault_path, opts);
    REQUIRE(mounted.is_ok());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    REQUIRE(std::filesystem::exists(mount_dir));

    const std::string payload = "written from os";
    const auto os_file = std::filesystem::path(mount_dir.generic_string() + "\\oswrite.txt");
    {
        HANDLE handle = CreateFileW(
            os_file.wstring().c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        REQUIRE(handle != INVALID_HANDLE_VALUE);
        DWORD written = 0;
        REQUIRE(WriteFile(
            handle,
            payload.data(),
            static_cast<DWORD>(payload.size()),
            &written,
            nullptr));
        REQUIRE(written == payload.size());
        CloseHandle(handle);
    }

    const auto* unlocked = sv::vault::SessionStore::instance().find(vault_path).value();
    const auto got = engine.get(*const_cast<sv::vault::UnlockedVault*>(unlocked), "/oswrite.txt");
    REQUIRE(got.is_ok());
    REQUIRE(std::string(got.value().begin(), got.value().end()) == payload);

    REQUIRE(manager.dismount(mounted.value()).is_ok());
    cleanup_mount_dir(mount_dir);
    sv::test::cleanup_vault(vault_path);
}

TEST_CASE("MT-03 dismount releases mount session", "[integration][mount][mt03]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Mount integration tests run in Release builds");
    }
#if !defined(SV_MOUNT_BACKEND_AVAILABLE)
    SKIP("FUSE/WinFsp mount backend not available in this build");
#endif

    const auto vault_path = sv::test::temp_vault_path("sv_mt03");
    (void)sv::mount::MountManager::instance().dismount_all();
    const auto mount_dir = pick_mount_point();
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(vault_path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions open_opts {};
    auto session = engine.open(vault_path, sv::test::outer_credentials(), open_opts);
    REQUIRE(session.is_ok());
    REQUIRE(sv::vault::SessionStore::instance().insert(std::move(session.value()), true).is_ok());

    sv::mount::MountOptions opts {};
    opts.mount_point = mount_dir;
    auto& manager = sv::mount::MountManager::instance();
    const auto mounted = manager.mount_vault(vault_path, opts);
    REQUIRE(mounted.is_ok());
    REQUIRE(manager.list_sessions().size() >= 1);

    REQUIRE(manager.dismount(mounted.value()).is_ok());
    bool found = false;
    for (const auto& active : manager.list_sessions()) {
        if (active.mount_point == mount_dir) {
            found = true;
        }
    }
    REQUIRE_FALSE(found);

    cleanup_mount_dir(mount_dir);
    sv::test::cleanup_vault(vault_path);
}

TEST_CASE("MT-04 mount stress small files", "[integration][mount][mt04]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Mount integration tests run in Release builds");
    }
#if !defined(SV_MOUNT_BACKEND_AVAILABLE)
    SKIP("FUSE/WinFsp mount backend not available in this build");
#endif

    const auto vault_path = sv::test::temp_vault_path("sv_mt04");
    (void)sv::mount::MountManager::instance().dismount_all();
    const auto mount_dir = pick_mount_point();
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(vault_path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions open_opts {};
    auto session = engine.open(vault_path, sv::test::outer_credentials(), open_opts);
    REQUIRE(session.is_ok());
    REQUIRE(sv::vault::SessionStore::instance().insert(std::move(session.value()), true).is_ok());

    sv::mount::MountOptions opts {};
    opts.mount_point = mount_dir;
    auto& manager = sv::mount::MountManager::instance();
    const auto mounted = manager.mount_vault(vault_path, opts);
    REQUIRE(mounted.is_ok());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    REQUIRE(std::filesystem::exists(mount_dir));

    const auto* unlocked = sv::vault::SessionStore::instance().find(vault_path).value();
    for (int i = 0; i < 100; ++i) {
        const std::string payload = "file-" + std::to_string(i);
        REQUIRE(engine.put(
            *const_cast<sv::vault::UnlockedVault*>(unlocked),
            "/f" + std::to_string(i) + ".txt",
            sv::Span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(payload.data()),
                payload.size()),
            false).is_ok());
    }

    for (int i = 0; i < 100; ++i) {
        const auto file = std::filesystem::path(
            mount_dir.generic_string() + "\\f" + std::to_string(i) + ".txt");
        HANDLE handle = CreateFileW(
            file.wstring().c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        REQUIRE(handle != INVALID_HANDLE_VALUE);
        char buffer[32] {};
        DWORD read = 0;
        REQUIRE(ReadFile(handle, buffer, sizeof(buffer) - 1, &read, nullptr));
        CloseHandle(handle);
        REQUIRE(std::string(buffer, read) == "file-" + std::to_string(i));
    }

    REQUIRE(manager.dismount(mounted.value()).is_ok());
    cleanup_mount_dir(mount_dir);
    sv::test::cleanup_vault(vault_path);
}

TEST_CASE("MT-05 auto-dismount idle", "[integration][mount][mt05]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Mount integration tests run in Release builds");
    }
#if !defined(SV_MOUNT_BACKEND_AVAILABLE)
    SKIP("FUSE/WinFsp mount backend not available in this build");
#endif

    const auto vault_path = sv::test::temp_vault_path("sv_mt05");
    (void)sv::mount::MountManager::instance().dismount_all();
    const auto mount_dir = pick_mount_point();
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(vault_path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions open_opts {};
    auto session = engine.open(vault_path, sv::test::outer_credentials(), open_opts);
    REQUIRE(session.is_ok());
    REQUIRE(sv::vault::SessionStore::instance().insert(std::move(session.value()), true).is_ok());

    sv::mount::MountOptions opts {};
    opts.mount_point = mount_dir;
    opts.idle_timeout = std::chrono::seconds(1);
    opts.auto_dismount = true;
    auto& manager = sv::mount::MountManager::instance();
    const auto mounted = manager.mount_vault(vault_path, opts);
    REQUIRE(mounted.is_ok());

    std::this_thread::sleep_for(std::chrono::seconds(2));
    manager.on_idle_tick();

    bool found = false;
    for (const auto& active : manager.list_sessions()) {
        if (active.mount_point == mount_dir) {
            found = true;
        }
    }
    REQUIRE_FALSE(found);

    cleanup_mount_dir(mount_dir);
    sv::test::cleanup_vault(vault_path);
}