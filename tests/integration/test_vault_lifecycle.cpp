#include <sv/crypto/kdf_engine.hpp>
#include <sv/vault/credential_material.hpp>
#include <sv/vault/vault_engine.hpp>
#include <sv/vault/verify.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>

namespace {

std::filesystem::path temp_vault_path() {
    static std::atomic<std::uint64_t> counter {0};
    const auto stamp = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto id = counter.fetch_add(1, std::memory_order_relaxed);
    return std::filesystem::path("C:/Temp")
        / ("sv_test_lifecycle_" + std::to_string(stamp) + "_" + std::to_string(id) + ".svlt");
}

sv::vault::CredentialMaterial test_credentials() {
    sv::vault::CredentialMaterial cred {};
    cred.password_utf8 = "integration-test-password-12";
    return cred;
}

sv::vault::CreateVaultOptions fast_test_options() {
    sv::vault::CreateVaultOptions options {};
    options.container_size = 4ULL * 1024 * 1024;
    options.chunk_size = 64 * 1024;
    options.kdf_params.m_cost_kib = 4096;
    options.kdf_params.t_cost = 1;
    options.kdf_params.p_cost = 1;
    return options;
}

void cleanup(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

} // namespace

TEST_CASE("vault create", "[integration][vault][create]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());

    const auto path = temp_vault_path();
    sv::vault::VaultEngine engine;
    const auto created = engine.create_new(path, test_credentials(), fast_test_options());
    if (!created.is_ok()) {
        INFO("create_new error: " << created.message());
    }
    REQUIRE(created.is_ok());
    cleanup(path);
}

TEST_CASE("vault create open", "[integration][vault][open]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());

    const auto path = temp_vault_path();
    const auto cred = test_credentials();
    const auto opts = fast_test_options();

    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, cred, opts).is_ok());

    sv::vault::OpenVaultOptions open_opts {};
    const auto session = engine.open(path, cred, open_opts);
    if (!session.is_ok()) {
        INFO("open error: " << session.message());
    }
    REQUIRE(session.is_ok());
    cleanup(path);
}

TEST_CASE("vault lifecycle integration", "[integration][vault][lifecycle]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());

#if !defined(NDEBUG)
    SKIP("Full lifecycle is too slow in Debug MSVC builds; run Release [integration] for end-to-end coverage");
#endif

    const auto path = temp_vault_path();
    sv::vault::VaultEngine engine;
    const auto cred = test_credentials();
    const auto opts = fast_test_options();

    const auto created = engine.create_new(path, cred, opts);
    REQUIRE(created.is_ok());

    sv::vault::OpenVaultOptions open_opts {};
    auto session = engine.open(path, cred, open_opts);
    REQUIRE(session.is_ok());

    const std::string payload = "hello shadow vault";
    const auto put = engine.put(
        session.value(),
        "/hello.txt",
        sv::Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(payload.data()),
            payload.size()));
    if (!put.is_ok()) {
        INFO("put error: " << put.message());
    }
    REQUIRE(put.is_ok());

    const auto listed = engine.list(session.value(), "/");
    REQUIRE(listed.is_ok());
    REQUIRE_FALSE(listed.value().empty());

    const auto got = engine.get(session.value(), "/hello.txt");
    REQUIRE(got.is_ok());
    REQUIRE(std::string(got.value().begin(), got.value().end()) == payload);

    const auto report = sv::vault::verify_session(session.value());
    REQUIRE(report.is_ok());
    REQUIRE(report.value().structural_ok);
    REQUIRE(report.value().crypto_ok);

    cleanup(path);
}