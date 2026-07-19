#include "test_fixtures.hpp"

#include <sv/vault/hidden_volume.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace {

constexpr std::uint64_t kHiddenSize = 32ULL * 1024 * 1024;

} 

TEST_CASE("HV-01 create hidden in empty outer", "[integration][hidden][hv01]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Hidden volume integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_hv01");
    sv::vault::VaultEngine engine;
    const auto outer_cred = sv::test::outer_credentials();
    const auto hidden_cred = sv::test::hidden_credentials();
    const auto opts = sv::test::hidden_test_create_options();

    REQUIRE(engine.create_new(path, outer_cred, opts).is_ok());

    sv::vault::OpenVaultOptions outer_opts {};
    auto outer = engine.open(path, outer_cred, outer_opts);
    REQUIRE(outer.is_ok());

    const auto created = sv::vault::create_hidden_volume(engine, outer.value(), hidden_cred, kHiddenSize);
    if (!created.is_ok()) {
        INFO("create_hidden_volume error: " << created.message());
    }
    REQUIRE(created.is_ok());
    REQUIRE(outer.value().header.hidden_max_size == kHiddenSize);
    REQUIRE(outer.value().header.hidden_present == 1);
    REQUIRE(outer.value().hidden_configured);
    sv::test::release_session(outer.value());

    sv::test::cleanup_vault(path);
}

TEST_CASE("HV-02 hidden unlock lists empty root", "[integration][hidden][hv02]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Hidden volume integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_hv02");
    sv::vault::VaultEngine engine;
    const auto outer_cred = sv::test::outer_credentials();
    const auto hidden_cred = sv::test::hidden_credentials();
    const auto opts = sv::test::hidden_test_create_options();

    REQUIRE(engine.create_new(path, outer_cred, opts).is_ok());

    sv::vault::OpenVaultOptions outer_opts {};
    auto outer = engine.open(path, outer_cred, outer_opts);
    REQUIRE(outer.is_ok());
    REQUIRE(sv::vault::create_hidden_volume(engine, outer.value(), hidden_cred, kHiddenSize).is_ok());
    sv::test::release_session(outer.value());

    sv::vault::OpenVaultOptions hidden_opts {};
    hidden_opts.intent = sv::vault::HeaderSlotIntent::Hidden;
    auto hidden = engine.open(path, hidden_cred, hidden_opts);
    REQUIRE(hidden.is_ok());
    const auto listed = engine.list(hidden.value(), "/");
    REQUIRE(listed.is_ok());
    REQUIRE(listed.value().empty());

    sv::test::cleanup_vault(path);
}

TEST_CASE("HV-05 hidden put get roundtrip", "[integration][hidden][hv05]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Hidden volume integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_hv05");
    sv::vault::VaultEngine engine;
    const auto outer_cred = sv::test::outer_credentials();
    const auto hidden_cred = sv::test::hidden_credentials();
    const auto opts = sv::test::hidden_test_create_options();

    REQUIRE(engine.create_new(path, outer_cred, opts).is_ok());

    sv::vault::OpenVaultOptions outer_opts {};
    auto outer = engine.open(path, outer_cred, outer_opts);
    REQUIRE(outer.is_ok());
    REQUIRE(sv::vault::create_hidden_volume(engine, outer.value(), hidden_cred, kHiddenSize).is_ok());
    sv::test::release_session(outer.value());

    sv::vault::OpenVaultOptions hidden_opts {};
    hidden_opts.intent = sv::vault::HeaderSlotIntent::Hidden;
    auto hidden = engine.open(path, hidden_cred, hidden_opts);
    REQUIRE(hidden.is_ok());
    const std::string payload = "hidden vault secret payload";
    const auto put = engine.put(
        hidden.value(),
        "/secret.txt",
        sv::Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(payload.data()),
            payload.size()));
    if (!put.is_ok()) {
        INFO("hidden put error: " << put.message());
    }
    REQUIRE(put.is_ok());

    const auto got = engine.get(hidden.value(), "/secret.txt");
    REQUIRE(got.is_ok());
    REQUIRE(std::string(got.value().begin(), got.value().end()) == payload);

    sv::test::cleanup_vault(path);
}