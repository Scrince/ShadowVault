#include <sv/crypto/crypto_provider.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>
#include <vector>

namespace {

constexpr std::array<std::uint8_t, 32> kKey {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
};

constexpr std::array<std::uint8_t, 24> kNonce {
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB,
    0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
};

constexpr char kPlaintext[] = "Shadow Vault AEAD known-answer vector";

} // namespace

TEST_CASE("C-01 AEAD encrypt/decrypt roundtrip", "[unit][crypto][c01]") {
    auto provider = sv::crypto::create_default_crypto_provider();
    REQUIRE(provider != nullptr);

    sv::crypto::AeadKey key {};
    std::memcpy(key.bytes.data(), kKey.data(), kKey.size());
    sv::crypto::AeadNonce nonce {};
    std::memcpy(nonce.bytes.data(), kNonce.data(), kNonce.size());

    const auto plaintext = sv::Span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(kPlaintext),
        sizeof(kPlaintext) - 1);
    std::vector<std::uint8_t> ciphertext(plaintext.size());
    std::array<std::uint8_t, sv::crypto::kAeadTagSize> tag {};

    const auto enc = provider->aead_encrypt(
        plaintext,
        sv::Span<const std::uint8_t> {},
        key,
        nonce,
        sv::Span<std::uint8_t>(ciphertext.data(), ciphertext.size()),
        sv::Span<std::uint8_t>(tag.data(), tag.size()));
    REQUIRE(enc.is_ok());

    std::vector<std::uint8_t> decrypted(plaintext.size());
    const auto dec = provider->aead_decrypt(
        sv::Span<const std::uint8_t>(ciphertext.data(), ciphertext.size()),
        sv::Span<const std::uint8_t> {},
        key,
        nonce,
        sv::Span<const std::uint8_t>(tag.data(), tag.size()),
        sv::Span<std::uint8_t>(decrypted.data(), decrypted.size()));
    REQUIRE(dec.is_ok());
    REQUIRE(std::memcmp(decrypted.data(), plaintext.data(), plaintext.size()) == 0);
}

TEST_CASE("C-01 AEAD output is deterministic for fixed key and nonce", "[unit][crypto][c01]") {
    auto provider = sv::crypto::create_default_crypto_provider();
    REQUIRE(provider != nullptr);

    sv::crypto::AeadKey key {};
    std::memcpy(key.bytes.data(), kKey.data(), kKey.size());
    sv::crypto::AeadNonce nonce {};
    std::memcpy(nonce.bytes.data(), kNonce.data(), kNonce.size());

    const auto plaintext = sv::Span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(kPlaintext),
        sizeof(kPlaintext) - 1);
    std::vector<std::uint8_t> ciphertext_a(plaintext.size());
    std::vector<std::uint8_t> ciphertext_b(plaintext.size());
    std::array<std::uint8_t, sv::crypto::kAeadTagSize> tag_a {};
    std::array<std::uint8_t, sv::crypto::kAeadTagSize> tag_b {};

    REQUIRE(provider->aead_encrypt(
                plaintext,
                sv::Span<const std::uint8_t> {},
                key,
                nonce,
                sv::Span<std::uint8_t>(ciphertext_a.data(), ciphertext_a.size()),
                sv::Span<std::uint8_t>(tag_a.data(), tag_a.size()))
                .is_ok());
    REQUIRE(provider->aead_encrypt(
                plaintext,
                sv::Span<const std::uint8_t> {},
                key,
                nonce,
                sv::Span<std::uint8_t>(ciphertext_b.data(), ciphertext_b.size()),
                sv::Span<std::uint8_t>(tag_b.data(), tag_b.size()))
                .is_ok());

    REQUIRE(ciphertext_a == ciphertext_b);
    REQUIRE(tag_a == tag_b);
}

TEST_CASE("C-01 AEAD rejects wrong key", "[unit][crypto][c01]") {
    auto provider = sv::crypto::create_default_crypto_provider();
    REQUIRE(provider != nullptr);

    sv::crypto::AeadKey key {};
    std::memcpy(key.bytes.data(), kKey.data(), kKey.size());
    sv::crypto::AeadNonce nonce {};
    std::memcpy(nonce.bytes.data(), kNonce.data(), kNonce.size());

    const auto plaintext = sv::Span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(kPlaintext),
        sizeof(kPlaintext) - 1);
    std::vector<std::uint8_t> ciphertext(plaintext.size());
    std::array<std::uint8_t, sv::crypto::kAeadTagSize> tag {};

    REQUIRE(provider->aead_encrypt(
                plaintext,
                sv::Span<const std::uint8_t> {},
                key,
                nonce,
                sv::Span<std::uint8_t>(ciphertext.data(), ciphertext.size()),
                sv::Span<std::uint8_t>(tag.data(), tag.size()))
                .is_ok());

    key.bytes[0] ^= 0x01;
    std::vector<std::uint8_t> decrypted(plaintext.size());
    const auto dec = provider->aead_decrypt(
        sv::Span<const std::uint8_t>(ciphertext.data(), ciphertext.size()),
        sv::Span<const std::uint8_t> {},
        key,
        nonce,
        sv::Span<const std::uint8_t>(tag.data(), tag.size()),
        sv::Span<std::uint8_t>(decrypted.data(), decrypted.size()));
    REQUIRE_FALSE(dec.is_ok());
}