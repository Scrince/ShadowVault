#include <sv/crypto/kdf_engine.hpp>

#include "sodium_init.hpp"

#include <sodium.h>

#include <array>

namespace sv::crypto {

namespace {

constexpr std::array<unsigned char, 24> kSaltDeriveKey {
    'S', 'h', 'a', 'd', 'o', 'w', 'V', 'a',
    'u', 'l', 't', '-', 'K', 'D', 'F', '-',
    'S', 'a', 'l', 't', '-', 'v', '1', 0,
};

[[nodiscard]] Result<std::array<std::uint8_t, crypto_pwhash_argon2id_SALTBYTES>>
normalize_argon2id_salt(const Span<const std::uint8_t> salt) noexcept {
    if (salt.empty()) {
        return Result<std::array<std::uint8_t, crypto_pwhash_argon2id_SALTBYTES>>::err(
            ErrorCode::InvalidArgument,
            "salt must not be empty");
    }

    std::array<std::uint8_t, crypto_pwhash_argon2id_SALTBYTES> normalized {};
    if (crypto_generichash_blake2b(
            normalized.data(),
            normalized.size(),
            salt.data(),
            salt.size(),
            kSaltDeriveKey.data(),
            kSaltDeriveKey.size())
        != 0) {
        return Result<std::array<std::uint8_t, crypto_pwhash_argon2id_SALTBYTES>>::err(
            ErrorCode::PlatformError,
            "salt normalization failed");
    }

    return Result<std::array<std::uint8_t, crypto_pwhash_argon2id_SALTBYTES>>::ok(normalized);
}

} 

bool KdfEngine::is_available() noexcept {
    return detail::ensure_sodium_initialized();
}

Result<void> KdfEngine::argon2id(
    Span<const std::uint8_t> password,
    Span<const std::uint8_t> salt,
    KdfParams params,
    Span<std::uint8_t> derived_key_out) noexcept {
    if (!detail::ensure_sodium_initialized()) {
        return Result<void>::err(ErrorCode::PlatformError, "libsodium initialization failed");
    }
    if (derived_key_out.empty()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "derived key output must not be empty");
    }

    const auto normalized_salt = normalize_argon2id_salt(salt);
    if (!normalized_salt.is_ok()) {
        return Result<void>::err(normalized_salt.error(), normalized_salt.message());
    }

    
    constexpr std::uint32_t kMaxMemoryKiB = 1048576; 
    constexpr std::uint32_t kMaxTimeCost = 10;
    std::uint32_t m_kib = params.m_cost_kib;
    std::uint32_t t_cost = params.t_cost;
    if (m_kib < 8192U) {
        m_kib = 8192U;
    }
    if (m_kib > kMaxMemoryKiB) {
        m_kib = kMaxMemoryKiB;
    }
    if (t_cost < 1U) {
        t_cost = 1U;
    }
    if (t_cost > kMaxTimeCost) {
        t_cost = kMaxTimeCost;
    }

    if (crypto_pwhash_argon2id(
            derived_key_out.data(),
            static_cast<unsigned long long>(derived_key_out.size()),
            reinterpret_cast<const char*>(password.data()),
            static_cast<unsigned long long>(password.size()),
            normalized_salt.value().data(),
            static_cast<unsigned long long>(t_cost),
            static_cast<std::size_t>(m_kib) * 1024U,
            crypto_pwhash_argon2id_ALG_ARGON2ID13)
        != 0) {
        return Result<void>::err(ErrorCode::PlatformError, "crypto_pwhash_argon2id failed");
    }
    return Result<void>::ok();
}

} 
