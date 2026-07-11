#include <sv/crypto/constant_time.hpp>
#include <sv/crypto/crypto_provider.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>

TEST_CASE("C-05 constant_time_compare accepts equal buffers", "[unit][crypto][c05]") {
    const std::array<std::uint8_t, 8> left {1, 2, 3, 4, 5, 6, 7, 8};
    std::array<std::uint8_t, 8> right = left;
    REQUIRE(sv::crypto::constant_time_compare(left, right));
}

TEST_CASE("C-05 constant_time_compare rejects unequal buffers", "[unit][crypto][c05]") {
    const std::array<std::uint8_t, 8> left {1, 2, 3, 4, 5, 6, 7, 8};
    std::array<std::uint8_t, 8> right = left;
    right[7] ^= 0x01;
    REQUIRE_FALSE(sv::crypto::constant_time_compare(left, right));
}

TEST_CASE("C-05 constant_time_compare rejects length mismatch", "[unit][crypto][c05]") {
    const std::array<std::uint8_t, 8> left {1, 2, 3, 4, 5, 6, 7, 8};
    const sv::Span<const std::uint8_t> shorter {left.data(), 4};
    REQUIRE_FALSE(sv::crypto::constant_time_compare(left, shorter));
}

TEST_CASE("C-05 crypto provider constant_time_eq mirrors compare", "[unit][crypto][c05]") {
    auto provider = sv::crypto::create_default_crypto_provider();
    REQUIRE(provider != nullptr);

    const std::array<std::uint8_t, 4> left {9, 8, 7, 6};
    std::array<std::uint8_t, 4> right = left;
    REQUIRE(provider->constant_time_eq(left, right));

    right[0] = 0;
    REQUIRE_FALSE(provider->constant_time_eq(left, right));
}