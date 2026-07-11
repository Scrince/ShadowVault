#include <sv/crypto/constant_time.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/crypto/secure_random.hpp>
#include <sv/span.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

using namespace sv::crypto;

TEST_CASE("secure_random_bytes produces non-zero output", "[crypto][smoke]") {
    std::array<std::uint8_t, 32> buffer{};
    const auto result = secure_random_bytes(buffer);
    REQUIRE(result.is_ok());

    bool all_zero = true;
    for (const auto byte : buffer) {
        if (byte != 0) {
            all_zero = false;
            break;
        }
    }
    REQUIRE_FALSE(all_zero);
}

TEST_CASE("constant_time_compare matches equal and rejects unequal buffers", "[crypto][smoke]") {
    const std::array<std::uint8_t, 4> left{1, 2, 3, 4};
    std::array<std::uint8_t, 4> right{1, 2, 3, 4};

    REQUIRE(constant_time_compare(left, right));

    right[3] = 5;
    REQUIRE_FALSE(constant_time_compare(left, right));

    const sv::Span<const std::uint8_t> shorter{right.data(), 3};
    REQUIRE_FALSE(constant_time_compare(left, shorter));
}

TEST_CASE("create_default_crypto_provider initializes libsodium backend", "[crypto][smoke]") {
    const auto provider = create_default_crypto_provider();
    REQUIRE(provider != nullptr);

    std::array<std::uint8_t, 16> random_out{};
    const auto random_result = provider->random_bytes(random_out);
    REQUIRE(random_result.is_ok());
}