#include <sv/crypto/kdf_engine.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstring>

namespace {

constexpr char kPassword[] = "test-vector-password-do-not-use";

std::array<std::uint8_t, 32> sample_salt() {
    std::array<std::uint8_t, 32> salt {};
    for (std::size_t i = 0; i < salt.size(); ++i) {
        salt[i] = static_cast<std::uint8_t>(0x10 + i);
    }
    return salt;
}

sv::crypto::KdfParams fast_kdf_params() {
    sv::crypto::KdfParams params {};
    params.m_cost_kib = 8;
    params.t_cost = 1;
    params.p_cost = 1;
    return params;
}

} // namespace

TEST_CASE("C-02 Argon2id known answer is deterministic", "[unit][crypto][c02]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());

    const auto salt = sample_salt();
    const auto params = fast_kdf_params();
    const auto password = sv::Span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(kPassword),
        sizeof(kPassword) - 1);

    std::array<std::uint8_t, 64> derived_a {};
    std::array<std::uint8_t, 64> derived_b {};
    REQUIRE(sv::crypto::KdfEngine::argon2id(password, salt, params, derived_a).is_ok());
    REQUIRE(sv::crypto::KdfEngine::argon2id(password, salt, params, derived_b).is_ok());
    REQUIRE(derived_a == derived_b);
    REQUIRE_FALSE(std::all_of(derived_a.begin(), derived_a.end(), [](const std::uint8_t b) { return b == 0; }));
}

TEST_CASE("C-02 Argon2id rejects empty salt", "[unit][crypto][c02]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());

    const auto password = sv::Span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(kPassword),
        sizeof(kPassword) - 1);
    std::array<std::uint8_t, 32> derived {};
    const auto result = sv::crypto::KdfEngine::argon2id(
        password,
        sv::Span<const std::uint8_t> {},
        fast_kdf_params(),
        derived);
    REQUIRE_FALSE(result.is_ok());
    REQUIRE(result.error() == sv::ErrorCode::InvalidArgument);
}

TEST_CASE("C-02 Argon2id rejects empty output buffer", "[unit][crypto][c02]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());

    const auto salt = sample_salt();
    const auto password = sv::Span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(kPassword),
        sizeof(kPassword) - 1);
    const auto result = sv::crypto::KdfEngine::argon2id(
        password,
        salt,
        fast_kdf_params(),
        sv::Span<std::uint8_t> {});
    REQUIRE_FALSE(result.is_ok());
    REQUIRE(result.error() == sv::ErrorCode::InvalidArgument);
}

TEST_CASE("C-02 Argon2id binds the full 32-byte vault salt", "[unit][crypto][c02]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());

    auto salt_a = sample_salt();
    auto salt_b = salt_a;
    salt_b[31] ^= 0x80;

    const auto password = sv::Span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(kPassword),
        sizeof(kPassword) - 1);

    std::array<std::uint8_t, 64> derived_a {};
    std::array<std::uint8_t, 64> derived_b {};
    REQUIRE(sv::crypto::KdfEngine::argon2id(password, salt_a, fast_kdf_params(), derived_a).is_ok());
    REQUIRE(sv::crypto::KdfEngine::argon2id(password, salt_b, fast_kdf_params(), derived_b).is_ok());
    REQUIRE(derived_a != derived_b);
}
