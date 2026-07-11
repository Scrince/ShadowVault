#include <sv/crypto/kdf_engine.hpp>

#include "sodium_init.hpp"

#include <sodium.h>

#include <array>
#include <cstring>

#ifdef SV_HAS_ARGON2
#include <argon2.h>
#endif

namespace sv::crypto {

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
    if (salt.empty()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "salt must not be empty");
    }

#ifdef SV_HAS_ARGON2
    const int rc = argon2id_hash_raw(
        params.t_cost,
        params.m_cost_kib,
        params.p_cost,
        password.data(),
        password.size(),
        salt.data(),
        salt.size(),
        derived_key_out.data(),
        derived_key_out.size());

    if (rc != ARGON2_OK) {
        return Result<void>::err(ErrorCode::PlatformError, argon2_error_message(rc));
    }
    return Result<void>::ok();
#else
    if (salt.size() < crypto_pwhash_argon2id_SALTBYTES) {
        return Result<void>::err(ErrorCode::InvalidArgument, "salt too short for libsodium argon2id");
    }

    std::array<std::uint8_t, crypto_pwhash_argon2id_SALTBYTES> salt16 {};
    std::memcpy(salt16.data(), salt.data(), salt16.size());

    if (crypto_pwhash_argon2id(
            derived_key_out.data(),
            static_cast<unsigned long long>(derived_key_out.size()),
            reinterpret_cast<const char*>(password.data()),
            static_cast<unsigned long long>(password.size()),
            salt16.data(),
            static_cast<unsigned long long>(params.t_cost),
            static_cast<std::size_t>(params.m_cost_kib) * 1024U,
            crypto_pwhash_argon2id_ALG_ARGON2ID13)
        != 0) {
        return Result<void>::err(ErrorCode::PlatformError, "crypto_pwhash_argon2id failed");
    }
    return Result<void>::ok();
#endif
}

} // namespace sv::crypto