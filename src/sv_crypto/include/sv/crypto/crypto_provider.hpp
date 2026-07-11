#pragma once

#include <sv/result.hpp>
#include <sv/span.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace sv::crypto {

struct AeadKey {
    std::array<std::uint8_t, 32> bytes{};
};

struct AeadNonce {
    std::array<std::uint8_t, 24> bytes{};
};

constexpr std::size_t kAeadKeySize = 32;
constexpr std::size_t kAeadNonceSize = 24;
constexpr std::size_t kAeadTagSize = 16;

struct KdfParams {
    std::uint32_t m_cost_kib = 262144;
    std::uint32_t t_cost = 3;
    std::uint32_t p_cost = 1;
    std::uint32_t parallelism_reserved = 0;
};

class CryptoProvider {
public:
    virtual ~CryptoProvider() = default;

    virtual Result<void> aead_encrypt(
        Span<const std::uint8_t> plaintext,
        Span<const std::uint8_t> associated_data,
        const AeadKey& key,
        const AeadNonce& nonce,
        Span<std::uint8_t> ciphertext_out,
        Span<std::uint8_t> tag_out) = 0;

    virtual Result<void> aead_decrypt(
        Span<const std::uint8_t> ciphertext,
        Span<const std::uint8_t> associated_data,
        const AeadKey& key,
        const AeadNonce& nonce,
        Span<const std::uint8_t> tag,
        Span<std::uint8_t> plaintext_out) = 0;

    virtual Result<void> kdf_argon2id(
        Span<const std::uint8_t> password,
        Span<const std::uint8_t> salt,
        KdfParams params,
        Span<std::uint8_t> derived_key_out) = 0;

    virtual void secure_zero(Span<std::uint8_t> buffer) = 0;
    virtual Result<void> random_bytes(Span<std::uint8_t> out) = 0;
    virtual bool constant_time_eq(Span<const std::uint8_t> a, Span<const std::uint8_t> b) = 0;
};

/// Returns nullptr when the backend cannot be initialized (no throw).
[[nodiscard]] std::unique_ptr<CryptoProvider> create_default_crypto_provider() noexcept;

} // namespace sv::crypto