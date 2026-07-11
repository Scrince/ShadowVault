#include <sv/crypto/secure_buffer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using namespace sv::crypto;

TEST_CASE("C-06 SecureBuffer shrink preserves prefix", "[unit][crypto][c06]") {
    SecureBuffer buffer(8);
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        buffer.data()[i] = static_cast<std::uint8_t>(0xFFU);
    }

    buffer.resize(4);
    REQUIRE(buffer.size() == 4);
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        REQUIRE(buffer.data()[i] == 0xFFU);
    }
}

TEST_CASE("C-06 SecureBuffer clear releases storage", "[unit][crypto][c06]") {
    SecureBuffer buffer(32);
    buffer.data()[0] = 0x42;
    buffer.clear();
    REQUIRE(buffer.empty());
}

TEST_CASE("C-06 SecureBuffer move transfers ownership", "[unit][crypto][c06]") {
    SecureBuffer source(32);
    source.data()[0] = 0x11;
    SecureBuffer destination(std::move(source));
    REQUIRE(destination.size() == 32);
    REQUIRE(destination.data()[0] == 0x11);
    REQUIRE(source.empty());
}

TEST_CASE("C-06 SecureBuffer destructor is safe after secret write", "[unit][crypto][c06]") {
    auto make_buffer = []() {
        SecureBuffer buffer(64);
        for (std::size_t i = 0; i < buffer.size(); ++i) {
            buffer.data()[i] = static_cast<std::uint8_t>(0xA5);
        }
        return buffer;
    };
    REQUIRE_NOTHROW(make_buffer());
}