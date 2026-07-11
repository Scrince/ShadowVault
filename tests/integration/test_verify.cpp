#include "test_fixtures.hpp"

#include <sv/vault/verify.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

TEST_CASE("R-04 verify structural locked pass", "[integration][verify][r04]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Verify integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_r04");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    auto container = sv::container::ContainerFileStore::open(path);
    REQUIRE(container.is_ok());

    const auto report = sv::vault::verify_structural(container.value());
    REQUIRE(report.is_ok());
    REQUIRE(report.value().structural_ok);

    sv::test::cleanup_vault(path);
}

TEST_CASE("R-04b verify crypto shallow unlocked pass", "[integration][verify][r04b]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Verify integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_r04b");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    const auto report = sv::vault::verify_session(session.value(), false);
    REQUIRE(report.is_ok());
    REQUIRE(report.value().structural_ok);
    REQUIRE(report.value().crypto_ok);

    sv::test::cleanup_vault(path);
}

TEST_CASE("R-05 verify deep chunk failure", "[integration][verify][r05]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Verify integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_r05");
    sv::vault::VaultEngine engine;
    const auto create_opts = sv::test::file_ops_create_options();
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), create_opts).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    const std::string payload = "tamper target";
    REQUIRE(engine.put(
                session.value(),
                "/tamper.txt",
                sv::Span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(payload.data()),
                    payload.size()))
                .is_ok());

    const auto inode_it = std::find_if(
        session.value().manifest.inodes.begin(),
        session.value().manifest.inodes.end(),
        [](const sv::vault::Inode& inode) { return inode.name_utf8 == "tamper.txt"; });
    REQUIRE(inode_it != session.value().manifest.inodes.end());
    REQUIRE(inode_it->chunk_count > 0);

    const std::uint64_t offset = session.value().header.data_offset
        + inode_it->first_chunk_id
            * (static_cast<std::uint64_t>(create_opts.chunk_size) + sv::crypto::kAeadTagSize);
    std::vector<std::uint8_t> corrupt(1, 0x00);
    REQUIRE(session.value().container.write_region(offset, sv::as_bytes(corrupt)).is_ok());

    const auto report = sv::vault::verify_session(session.value(), true);
    REQUIRE(report.is_ok());
    REQUIRE(report.value().structural_ok);
    REQUIRE_FALSE(report.value().crypto_ok);

    sv::test::cleanup_vault(path);
}