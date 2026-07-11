#include <sv/crypto/kdf_engine.hpp>
#include <sv/crypto/secure_random.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>

TEST_CASE("C-04 secure_random_bytes accepts empty output", "[unit][crypto][c04]") {
    std::array<std::uint8_t, 0> empty {};
    const auto result = sv::crypto::secure_random_bytes(empty);
    REQUIRE(result.is_ok());
}

TEST_CASE("C-04 secure_random_bytes produces non-zero output", "[unit][crypto][c04]") {
    std::array<std::uint8_t, 32> buffer {};
    const auto result = sv::crypto::secure_random_bytes(buffer);
    REQUIRE(result.is_ok());
    REQUIRE(std::any_of(buffer.begin(), buffer.end(), [](const std::uint8_t b) { return b != 0; }));
}

TEST_CASE("C-04 secure_random_bytes outputs differ across calls", "[unit][crypto][c04]") {
    std::array<std::uint8_t, 32> first {};
    std::array<std::uint8_t, 32> second {};
    REQUIRE(sv::crypto::secure_random_bytes(first).is_ok());
    REQUIRE(sv::crypto::secure_random_bytes(second).is_ok());
    REQUIRE(first != second);
}

TEST_CASE("C-04 KdfEngine rejects invalid inputs", "[unit][crypto][c04]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());

    std::array<std::uint8_t, 16> password {};
    std::array<std::uint8_t, 32> salt {};
    std::array<std::uint8_t, 32> derived {};

    REQUIRE_FALSE(sv::crypto::KdfEngine::argon2id(
                      sv::Span<const std::uint8_t>(password.data(), password.size()),
                      sv::Span<const std::uint8_t> {},
                      sv::crypto::KdfParams {},
                      derived)
                      .is_ok());
    REQUIRE_FALSE(sv::crypto::KdfEngine::argon2id(
                      sv::Span<const std::uint8_t>(password.data(), password.size()),
                      salt,
                      sv::crypto::KdfParams {},
                      sv::Span<std::uint8_t> {})
                      .is_ok());
}