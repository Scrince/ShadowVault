#include "test_fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <vector>

#include <sodium.h>

namespace {

std::vector<std::uint8_t> bytes_of_size(std::size_t size, std::uint8_t fill = 0x5A) {
    return std::vector<std::uint8_t>(size, fill);
}

} 

TEST_CASE("F-01 put get small file", "[integration][file][f01]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("File ops integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_f01");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    const std::string payload = "small file payload";
    REQUIRE(engine.put(
                session.value(),
                "/note.txt",
                sv::Span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(payload.data()),
                    payload.size()))
                .is_ok());

    const auto got = engine.get(session.value(), "/note.txt");
    REQUIRE(got.is_ok());
    REQUIRE(std::string(got.value().begin(), got.value().end()) == payload);

    sv::test::cleanup_vault(path);
}

TEST_CASE("F-02 put get large file multi-chunk", "[integration][file][f02]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("File ops integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_f02");
    sv::vault::VaultEngine engine;
    const auto create_opts = sv::test::file_ops_create_options();
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), create_opts).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    const std::size_t payload_size = static_cast<std::size_t>(create_opts.chunk_size) * 3 + 123;
    const auto payload = bytes_of_size(payload_size, 0xAB);
    REQUIRE(engine.put(
                session.value(),
                "/large.bin",
                sv::Span<const std::uint8_t>(payload.data(), payload.size()))
                .is_ok());

    const auto got = engine.get(session.value(), "/large.bin");
    REQUIRE(got.is_ok());
    REQUIRE(got.value() == payload);

    sv::test::cleanup_vault(path);
}

TEST_CASE("F-03 list directory", "[integration][file][f03]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("File ops integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_f03");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    REQUIRE(engine.put(
                session.value(),
                "/a.txt",
                sv::Span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>("a"), 1))
                .is_ok());
    REQUIRE(engine.put(
                session.value(),
                "/b.txt",
                sv::Span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>("b"), 1))
                .is_ok());

    const auto listed = engine.list(session.value(), "/");
    REQUIRE(listed.is_ok());
    REQUIRE(listed.value().size() == 2);
    REQUIRE(listed.value()[0].name == "a.txt");
    REQUIRE(listed.value()[1].name == "b.txt");

    sv::test::cleanup_vault(path);
}

TEST_CASE("F-04 remove file", "[integration][file][f04]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("File ops integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_f04");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    REQUIRE(engine.put(
                session.value(),
                "/gone.txt",
                sv::Span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>("x"), 1))
                .is_ok());
    REQUIRE(engine.remove(session.value(), "/gone.txt").is_ok());

    const auto got = engine.get(session.value(), "/gone.txt");
    REQUIRE_FALSE(got.is_ok());

    sv::test::cleanup_vault(path);
}

TEST_CASE("F-05 rename file", "[integration][file][f05]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("File ops integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_f05");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    const std::string payload = "rename me";
    REQUIRE(engine.put(
                session.value(),
                "/old.txt",
                sv::Span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(payload.data()),
                    payload.size()))
                .is_ok());
    REQUIRE(engine.rename(session.value(), "/old.txt", "/new.txt").is_ok());

    const auto got = engine.get(session.value(), "/new.txt");
    REQUIRE(got.is_ok());
    REQUIRE(std::string(got.value().begin(), got.value().end()) == payload);

    sv::test::cleanup_vault(path);
}

TEST_CASE("F-07 get first file after multiple puts", "[integration][file][f07]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("File ops integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_f07");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    for (int i = 0; i < 5; ++i) {
        const std::string payload = "payload-" + std::to_string(i);
        REQUIRE(engine.put(
                    session.value(),
                    "/f" + std::to_string(i) + ".txt",
                    sv::Span<const std::uint8_t>(
                        reinterpret_cast<const std::uint8_t*>(payload.data()),
                        payload.size()))
                    .is_ok());
    }

    for (int i = 0; i < 5; ++i) {
        const auto got = engine.get(session.value(), "/f" + std::to_string(i) + ".txt");
        REQUIRE(got.is_ok());
        REQUIRE(std::string(got.value().begin(), got.value().end()) == "payload-" + std::to_string(i));
    }

    sv::test::cleanup_vault(path);
}

TEST_CASE("F-06 mkdir nested", "[integration][file][f06]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("File ops integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_f06");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    REQUIRE(engine.mkdir(session.value(), "/projects/2026/demo").is_ok());

    const std::string payload = "nested path file";
    REQUIRE(engine.put(
                session.value(),
                "/projects/2026/demo/readme.txt",
                sv::Span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(payload.data()),
                    payload.size()))
                .is_ok());

    const auto listed = engine.list(session.value(), "/projects/2026/demo");
    REQUIRE(listed.is_ok());
    REQUIRE(listed.value().size() == 1);
    REQUIRE(listed.value().front().name == "readme.txt");

    sv::test::cleanup_vault(path);
}

TEST_CASE("F-07 remove recursive dir", "[integration][file][f07]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("File ops integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_f07");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    REQUIRE(engine.mkdir(session.value(), "/tree/branch").is_ok());
    REQUIRE(engine.put(
                session.value(),
                "/tree/branch/leaf.txt",
                sv::Span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>("leaf"), 4))
                .is_ok());
    REQUIRE(engine.remove(session.value(), "/tree").is_ok());

    const auto listed = engine.list(session.value(), "/");
    REQUIRE(listed.is_ok());
    REQUIRE(listed.value().empty());

    sv::test::cleanup_vault(path);
}

TEST_CASE("F-09 overwrite frees old chunks", "[integration][file][f09]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("File ops integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_f09");
    sv::vault::VaultEngine engine;
    const auto create_opts = sv::test::file_ops_create_options();
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), create_opts).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    const auto large = bytes_of_size(static_cast<std::size_t>(create_opts.chunk_size) * 2 + 1, 0xCD);
    const auto small = bytes_of_size(17, 0x42);

    REQUIRE(engine.put(
                session.value(),
                "/overwrite.bin",
                sv::Span<const std::uint8_t>(large.data(), large.size()),
                false)
                .is_ok());
    REQUIRE(engine.put(
                session.value(),
                "/overwrite.bin",
                sv::Span<const std::uint8_t>(small.data(), small.size()),
                true)
                .is_ok());
    REQUIRE(engine.put(
                session.value(),
                "/overwrite.bin",
                sv::Span<const std::uint8_t>(large.data(), large.size()),
                true)
                .is_ok());

    const auto got = engine.get(session.value(), "/overwrite.bin");
    REQUIRE(got.is_ok());
    REQUIRE(got.value() == large);

    sv::test::cleanup_vault(path);
}

TEST_CASE("F-10 read-only session rejects put", "[integration][file][f10]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("File ops integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_f10");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    opts.read_only = true;
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    const auto empty_put = engine.put(
        session.value(),
        "/empty.txt",
        sv::Span<const std::uint8_t> {},
        false);
    REQUIRE_FALSE(empty_put.is_ok());
    REQUIRE(empty_put.error() == sv::ErrorCode::InvalidArgument);

    sv::test::cleanup_vault(path);
}

TEST_CASE("F-gib 1 GiB file roundtrip hash match", "[integration][file][slow][fgib]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("File ops integration tests run in Release builds");
    }
    if (!sv::test::slow_tests_enabled()) {
        SKIP("Set SV_RUN_SLOW_TESTS=1 to run 1 GiB roundtrip");
    }

    constexpr std::size_t payload_size = 1024ULL * 1024 * 1024;
    const auto path = sv::test::temp_vault_path("sv_test_fgib");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::gib_file_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    std::vector<std::uint8_t> payload(payload_size);
    for (std::size_t i = 0; i < payload_size; ++i) {
        payload[i] = static_cast<std::uint8_t>((i * 1315423911U) >> 24);
    }

    std::array<std::uint8_t, crypto_generichash_BYTES> put_hash {};
    REQUIRE(crypto_generichash(
                put_hash.data(),
                put_hash.size(),
                payload.data(),
                payload.size(),
                nullptr,
                0)
            == 0);

    REQUIRE(engine.put(
                session.value(),
                "/one_gib.bin",
                sv::Span<const std::uint8_t>(payload.data(), payload.size()))
                .is_ok());
    payload.clear();
    payload.shrink_to_fit();

    const auto got = engine.get(session.value(), "/one_gib.bin");
    REQUIRE(got.is_ok());
    REQUIRE(got.value().size() == payload_size);

    std::array<std::uint8_t, crypto_generichash_BYTES> got_hash {};
    REQUIRE(crypto_generichash(
                got_hash.data(),
                got_hash.size(),
                got.value().data(),
                got.value().size(),
                nullptr,
                0)
            == 0);
    REQUIRE(got_hash == put_hash);

    sv::test::cleanup_vault(path);
}