#include <sv/crypto/hkdf.hpp>

#include "sodium_init.hpp"

#include <array>
#include <cstring>

namespace sv::crypto {

namespace {

void store_le64(std::uint8_t* out, std::uint64_t value) noexcept {
    for (std::size_t i = 0; i < sizeof(value); ++i) {
        out[i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFU);
    }
}

Result<AeadKey> hkdf_expand(
    Span<const std::uint8_t> ikm,
    Span<const std::uint8_t> salt,
    Span<const std::uint8_t> info) noexcept {
    if (!detail::ensure_sodium_initialized()) {
        return Result<AeadKey>::err(ErrorCode::PlatformError, "libsodium initialization failed");
    }

    std::array<std::uint8_t, crypto_kdf_hkdf_sha256_KEYBYTES> prk{};
    if (crypto_kdf_hkdf_sha256_extract(
            prk.data(),
            salt.data(),
            salt.size(),
            ikm.data(),
            ikm.size()) != 0) {
        sodium_memzero(prk.data(), prk.size());
        return Result<AeadKey>::err(ErrorCode::PlatformError, "HKDF-Extract failed");
    }

    AeadKey key{};
    if (crypto_kdf_hkdf_sha256_expand(
            key.bytes.data(),
            key.bytes.size(),
            reinterpret_cast<const char*>(info.data()),
            info.size(),
            prk.data()) != 0) {
        sodium_memzero(prk.data(), prk.size());
        sodium_memzero(key.bytes.data(), key.bytes.size());
        return Result<AeadKey>::err(ErrorCode::PlatformError, "HKDF-Expand failed");
    }

    sodium_memzero(prk.data(), prk.size());
    return Result<AeadKey>::ok(key);
}

} 

Result<AeadKey> hkdf_sha256(
    Span<const std::uint8_t> ikm,
    Span<const std::uint8_t> salt,
    Span<const std::uint8_t> info) noexcept {
    return hkdf_expand(ikm, salt, info);
}

Result<AeadKey> hkdf_chunk_key(Span<const std::uint8_t> vmk, std::uint64_t chunk_id) noexcept {
    std::array<std::uint8_t, 16> info{};
    std::memcpy(info.data(), "sv-ch-v1", 8);
    store_le64(info.data() + 8, chunk_id);
    return hkdf_sha256(vmk, Span<const std::uint8_t>{}, info);
}

Result<AeadKey> hkdf_manifest_page_key(
    Span<const std::uint8_t> vmk,
    std::uint64_t generation,
    std::uint64_t page_index) noexcept {
    std::array<std::uint8_t, 24> info{};
    std::memcpy(info.data(), "sv-mp-v1", 8);
    store_le64(info.data() + 8, generation);
    store_le64(info.data() + 16, page_index);
    return hkdf_sha256(vmk, Span<const std::uint8_t>{}, info);
}

Result<AeadKey> hkdf_journal_key(Span<const std::uint8_t> vmk, std::uint64_t sequence) noexcept {
    std::array<std::uint8_t, 16> info{};
    std::memcpy(info.data(), "sv-jr-v1", 8);
    store_le64(info.data() + 8, sequence);
    return hkdf_sha256(vmk, Span<const std::uint8_t>{}, info);
}

} 
