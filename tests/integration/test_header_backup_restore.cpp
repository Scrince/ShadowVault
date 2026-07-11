#include "test_fixtures.hpp"

#include <sv/recovery/repair.hpp>
#include <sv/vault/vault_engine.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("H-05 backup header restore unlocks vault", "[integration][header][h05]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Header restore integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_h05");
    sv::vault::VaultEngine engine;
    const auto opts = sv::test::hidden_test_create_options();

    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), opts).is_ok());
    REQUIRE(sv::test::corrupt_primary_outer_header(path).is_ok());

    {
        auto container = sv::container::ContainerFileStore::open(path);
        REQUIRE(container.is_ok());
        REQUIRE(sv::recovery::repair_from_backup_header(container.value()).is_ok());
    }

    sv::vault::OpenVaultOptions open_opts {};
    const auto session = engine.open(path, sv::test::outer_credentials(), open_opts);
    REQUIRE(session.is_ok());

    sv::test::cleanup_vault(path);
}

TEST_CASE("HV-09 repair preserves hidden header sector", "[integration][hidden][hv09]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Header restore integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_hv09");
    sv::vault::VaultEngine engine;
    REQUIRE(sv::test::create_hidden_test_vault(path, engine).is_ok());

    std::vector<std::uint8_t> hidden_before;
    {
        auto container = sv::container::ContainerFileStore::open(path);
        REQUIRE(container.is_ok());
        const auto sb = container.value().read_superblock_primary();
        REQUIRE(sb.is_ok());

        const auto hidden = container.value().read_region(
            sb.value().hidden_header_offset,
            sv::container::SVLT_HEADER_SIZE);
        REQUIRE(hidden.is_ok());
        hidden_before = hidden.value();
    }

    REQUIRE(sv::test::corrupt_primary_outer_header(path).is_ok());

    std::vector<std::uint8_t> hidden_after;
    {
        auto container = sv::container::ContainerFileStore::open(path);
        REQUIRE(container.is_ok());
        REQUIRE(sv::recovery::repair_from_backup_header(container.value()).is_ok());

        const auto sb_after = container.value().read_superblock_primary();
        REQUIRE(sb_after.is_ok());
        const auto hidden = container.value().read_region(
            sb_after.value().hidden_header_offset,
            sv::container::SVLT_HEADER_SIZE);
        REQUIRE(hidden.is_ok());
        hidden_after = hidden.value();
    }
    REQUIRE(hidden_after == hidden_before);

    sv::vault::OpenVaultOptions hidden_opts {};
    hidden_opts.intent = sv::vault::HeaderSlotIntent::Hidden;
    const auto hidden_session = engine.open(path, sv::test::hidden_credentials(), hidden_opts);
    REQUIRE(hidden_session.is_ok());

    sv::test::cleanup_vault(path);
}