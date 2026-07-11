#include "test_fixtures.hpp"

#include <sv/vault/verify.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE("R-03 truncated vault detected by verify", "[integration][recovery][r03]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Recovery integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_r03");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    std::uint64_t full_size = 0;
    {
        auto container = sv::container::ContainerFileStore::open(path);
        REQUIRE(container.is_ok());
        const auto sb = container.value().read_superblock_primary();
        REQUIRE(sb.is_ok());
        full_size = sb.value().container_size;
    }

    std::error_code ec;
    std::filesystem::resize_file(path, full_size / 2, ec);
    REQUIRE_FALSE(ec);

    auto container = sv::container::ContainerFileStore::open(path);
    REQUIRE(container.is_ok());
    REQUIRE(container.value().file_size() < full_size);

    const auto report = sv::vault::verify_structural(container.value());
    REQUIRE(report.is_ok());
    REQUIRE_FALSE(report.value().structural_ok);
    REQUIRE(report.value().message == "container file truncated");

    sv::test::cleanup_vault(path);
}