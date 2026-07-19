#include <sv/crypto/hkdf.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstring>

namespace {

std::array<std::uint8_t, 32> sample_vmk() {
    std::array<std::uint8_t, 32> vmk {};
    for (std::size_t i = 0; i < vmk.size(); ++i) {
        vmk[i] = static_cast<std::uint8_t>(0x40 + i);
    }
    return vmk;
}

bool is_nonzero(const sv::crypto::AeadKey& key) {
    return std::any_of(key.bytes.begin(), key.bytes.end(), [](const std::uint8_t b) { return b != 0; });
}

} 

TEST_CASE("C-03 hkdf_chunk_key is deterministic", "[unit][crypto][c03]") {
    const auto vmk = sample_vmk();
    const auto span = sv::Span<const std::uint8_t>(vmk.data(), vmk.size());

    const auto key_a = sv::crypto::hkdf_chunk_key(span, 42);
    const auto key_b = sv::crypto::hkdf_chunk_key(span, 42);
    REQUIRE(key_a.is_ok());
    REQUIRE(key_b.is_ok());
    REQUIRE(key_a.value().bytes == key_b.value().bytes);
    REQUIRE(is_nonzero(key_a.value()));
}

TEST_CASE("C-03 hkdf_chunk_key varies by chunk id", "[unit][crypto][c03]") {
    const auto vmk = sample_vmk();
    const auto span = sv::Span<const std::uint8_t>(vmk.data(), vmk.size());

    const auto key_one = sv::crypto::hkdf_chunk_key(span, 1);
    const auto key_two = sv::crypto::hkdf_chunk_key(span, 2);
    REQUIRE(key_one.is_ok());
    REQUIRE(key_two.is_ok());
    REQUIRE(key_one.value().bytes != key_two.value().bytes);
}

TEST_CASE("C-03 hkdf_manifest_page_key varies by generation and page", "[unit][crypto][c03]") {
    const auto vmk = sample_vmk();
    const auto span = sv::Span<const std::uint8_t>(vmk.data(), vmk.size());

    const auto page0 = sv::crypto::hkdf_manifest_page_key(span, 1, 0);
    const auto page1 = sv::crypto::hkdf_manifest_page_key(span, 1, 1);
    const auto gen2 = sv::crypto::hkdf_manifest_page_key(span, 2, 0);
    REQUIRE(page0.is_ok());
    REQUIRE(page1.is_ok());
    REQUIRE(gen2.is_ok());
    REQUIRE(page0.value().bytes != page1.value().bytes);
    REQUIRE(page0.value().bytes != gen2.value().bytes);
}

TEST_CASE("C-03 hkdf_journal_key varies by sequence", "[unit][crypto][c03]") {
    const auto vmk = sample_vmk();
    const auto span = sv::Span<const std::uint8_t>(vmk.data(), vmk.size());

    const auto seq1 = sv::crypto::hkdf_journal_key(span, 1);
    const auto seq2 = sv::crypto::hkdf_journal_key(span, 2);
    REQUIRE(seq1.is_ok());
    REQUIRE(seq2.is_ok());
    REQUIRE(seq1.value().bytes == sv::crypto::hkdf_journal_key(span, 1).value().bytes);
    REQUIRE(seq1.value().bytes != seq2.value().bytes);
}
