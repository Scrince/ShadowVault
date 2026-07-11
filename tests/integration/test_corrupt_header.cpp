#include "test_fixtures.hpp"

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