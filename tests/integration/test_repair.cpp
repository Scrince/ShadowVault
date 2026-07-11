#include "test_fixtures.hpp"

#include <sv/recovery/repair.hpp>
#include <sv/vault/vault_engine.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("HV-10 repair outer keeps hidden ciphertext", "[integration][hidden][hv10]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Repair integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_hv10");
    sv::vault::VaultEngine engine;
    REQUIRE(sv::test::create_hidden_test_vault(path, engine).is_ok());

    sv::vault::OpenVaultOptions hidden_opts {};
    hidden_opts.intent = sv::vault::HeaderSlotIntent::Hidden;
    auto hidden = engine.open(path, sv::test::hidden_credentials(), hidden_opts);
    REQUIRE(hidden.is_ok());

    const std::string payload = "hidden survives outer header repair";
    const auto put = engine.put(
        hidden.value(),
        "/survivor.txt",
        sv::Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(payload.data()),
            payload.size()));
    REQUIRE(put.is_ok());
    sv::test::release_session(hidden.value());

    REQUIRE(sv::test::corrupt_primary_outer_header(path).is_ok());

    {
        auto container = sv::container::ContainerFileStore::open(path);
        REQUIRE(container.is_ok());
        REQUIRE(sv::recovery::repair_from_backup_header(container.value()).is_ok());
    }

    hidden = engine.open(path, sv::test::hidden_credentials(), hidden_opts);
    REQUIRE(hidden.is_ok());

    const auto got = engine.get(hidden.value(), "/survivor.txt");
    REQUIRE(got.is_ok());
    REQUIRE(std::string(got.value().begin(), got.value().end()) == payload);

    sv::test::cleanup_vault(path);
}

TEST_CASE("R-06 repair dry-run does not modify container", "[integration][repair][r06]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Repair integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_r06");
    sv::vault::VaultEngine engine;
    const auto opts = sv::test::hidden_test_create_options();
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), opts).is_ok());

    const auto primary_before = sv::test::read_header_region(
        path,
        sv::container::SVLT_PRIMARY_HEADER_OFFSET);
    REQUIRE(primary_before.is_ok());

    REQUIRE(sv::test::corrupt_primary_outer_header(path).is_ok());

    {
        auto container = sv::container::ContainerFileStore::open(path);
        REQUIRE(container.is_ok());

        sv::recovery::RepairOptions dry_run_opts {};
        dry_run_opts.dry_run = true;
        REQUIRE(sv::recovery::repair_from_backup_header(container.value(), dry_run_opts).is_ok());
    }

    const auto primary_mid = sv::test::read_header_region(
        path,
        sv::container::SVLT_PRIMARY_HEADER_OFFSET);
    REQUIRE(primary_mid.is_ok());
    REQUIRE(primary_mid.value() == std::vector<std::uint8_t>(sv::container::SVLT_HEADER_SIZE, 0xFF));

    {
        auto container = sv::container::ContainerFileStore::open(path);
        REQUIRE(container.is_ok());
        REQUIRE(sv::recovery::repair_from_backup_header(container.value()).is_ok());
    }

    const auto primary_after = sv::test::read_header_region(
        path,
        sv::container::SVLT_PRIMARY_HEADER_OFFSET);
    REQUIRE(primary_after.is_ok());
    REQUIRE(primary_after.value() != primary_mid.value());

    sv::vault::OpenVaultOptions open_opts {};
    const auto session = engine.open(path, sv::test::outer_credentials(), open_opts);
    REQUIRE(session.is_ok());

    sv::test::cleanup_vault(path);
}