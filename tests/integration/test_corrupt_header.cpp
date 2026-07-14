#include "test_fixtures.hpp"

#include <sv/container/header_codec.hpp>
#include <sv/vault/vault_engine.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("R-02 corrupted primary header unlocks via backup", "[integration][recovery][r02]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Recovery integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_r02");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    REQUIRE(sv::test::corrupt_primary_outer_header(path).is_ok());

    sv::vault::OpenVaultOptions opts {};
    const auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    sv::test::cleanup_vault(path);
}

TEST_CASE("R-06 zero chunk size header is rejected on open", "[integration][recovery][r06]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Recovery integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_r06");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    auto payload = session.value().header;
    payload.chunk_size = 0;
    auto crypto = sv::crypto::create_default_crypto_provider();
    REQUIRE(crypto != nullptr);

    const auto primary = sv::container::seal_header_sector(
        *crypto,
        payload,
        session.value().header_key.span(),
        session.value().superblock,
        sv::container::HeaderSlotId::PrimaryOuter,
        sv::container::HeaderSlotType::Outer);
    REQUIRE(primary.is_ok());
    const auto backup = sv::container::seal_header_sector(
        *crypto,
        payload,
        session.value().header_key.span(),
        session.value().superblock,
        sv::container::HeaderSlotId::BackupOuter,
        sv::container::HeaderSlotType::Outer);
    REQUIRE(backup.is_ok());

    REQUIRE(session.value().container.write_header_sector(
                session.value().superblock.primary_header_offset,
                primary.value())
                .is_ok());
    REQUIRE(session.value().container.write_header_sector(
                session.value().superblock.backup_header_offset,
                backup.value())
                .is_ok());
    REQUIRE(session.value().container.sync().is_ok());
    sv::test::release_session(session.value());

    const auto reopened = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE_FALSE(reopened.is_ok());
    REQUIRE(reopened.error() == sv::ErrorCode::CorruptContainer);

    sv::test::cleanup_vault(path);
}
