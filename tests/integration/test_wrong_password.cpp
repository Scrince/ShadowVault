#include "test_fixtures.hpp"

#include <sv/vault/hidden_volume.hpp>

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr std::uint64_t kHiddenSize = 32ULL * 1024 * 1024;

} 

TEST_CASE("HV-06 wrong hidden password fails generic error", "[integration][hidden][hv06]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Hidden volume integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_hv06");
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

    sv::vault::CredentialMaterial wrong {};
    wrong.password_utf8 = "not-the-hidden-password";

    sv::vault::OpenVaultOptions hidden_opts {};
    hidden_opts.intent = sv::vault::HeaderSlotIntent::Hidden;
    const auto opened = engine.open(path, wrong, hidden_opts);
    REQUIRE_FALSE(opened.is_ok());
    REQUIRE(opened.error() == sv::ErrorCode::WrongCredentials);
    REQUIRE(opened.message() == "wrong credentials");

    sv::test::cleanup_vault(path);
}