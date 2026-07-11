#include <sv/mount/mount_types.hpp>
#include <sv/mount/svfs.hpp>
#include <sv/vault/session_store.hpp>
#include <sv/vault/vault_engine.hpp>

#include <catch2/catch_test_macros.hpp>

#include "test_fixtures.hpp"

TEST_CASE("MT-02 SvFs read-only denies write", "[integration][mount][mt02]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Mount integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_mt02");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions open_opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), open_opts);
    REQUIRE(session.is_ok());
    REQUIRE(sv::vault::SessionStore::instance().insert(std::move(session.value()), true).is_ok());

    const auto* unlocked = sv::vault::SessionStore::instance().find(path).value();
    sv::mount::MountOptions opts {};
    opts.read_only = true;
    sv::mount::SvFs svfs(engine, *const_cast<sv::vault::UnlockedVault*>(unlocked), opts);

    const auto mkdir = svfs.mkdir("/rodir");
    REQUIRE(!mkdir.is_ok());
    REQUIRE(mkdir.error() == sv::ErrorCode::InvalidArgument);

    REQUIRE(sv::vault::SessionStore::instance().remove(path).is_ok());
    sv::test::cleanup_vault(path);
}

TEST_CASE("MT-06 SvFs hidden protection refuses outer write", "[integration][mount][mt06]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Mount integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_mt06");
    sv::vault::VaultEngine engine;
    REQUIRE(sv::test::create_hidden_test_vault(path, engine).is_ok());

    sv::vault::OpenVaultOptions outer_opts {};
    outer_opts.protect_hidden = true;
    auto session = engine.open(path, sv::test::outer_credentials(), outer_opts);
    REQUIRE(session.is_ok());
    REQUIRE(sv::vault::SessionStore::instance().insert(std::move(session.value()), true).is_ok());

    const auto* unlocked = sv::vault::SessionStore::instance().find(path).value();
    sv::mount::MountOptions opts {};
    opts.protect_hidden = true;
    sv::mount::SvFs svfs(engine, *const_cast<sv::vault::UnlockedVault*>(unlocked), opts);

    std::vector<std::uint8_t> huge(8 * 1024 * 1024, 0xAB);
    const auto put = svfs.create_file("/fill.bin");
    REQUIRE(put.is_ok());

    const auto handle = svfs.open("/fill.bin", true);
    REQUIRE(handle.is_ok());
    const auto write = svfs.write(
        handle.value(),
        0,
        sv::Span<const std::uint8_t>(huge.data(), huge.size()));
    if (!write.is_ok()) {
        REQUIRE(write.error() == sv::ErrorCode::HiddenVolumeProtected);
    } else {
        svfs.release(handle.value());
    }

    REQUIRE(sv::vault::SessionStore::instance().remove(path).is_ok());
    sv::test::cleanup_vault(path);
}