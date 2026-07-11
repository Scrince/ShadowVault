#pragma once

#include <sv/crypto/crypto_provider.hpp>
#include <sv/span.hpp>

#include <cstdint>

namespace sv::crypto {

/// HKDF-SHA256 extract+expand producing a 32-byte AEAD key.
[[nodiscard]] AeadKey hkdf_sha256(
    Span<const std::uint8_t> ikm,
    Span<const std::uint8_t> salt,
    Span<const std::uint8_t> info) noexcept;

[[nodiscard]] AeadKey hkdf_chunk_key(Span<const std::uint8_t> vmk, std::uint64_t chunk_id) noexcept;
[[nodiscard]] AeadKey hkdf_manifest_page_key(
    Span<const std::uint8_t> vmk,
    std::uint64_t generation,
    std::uint64_t page_index) noexcept;
[[nodiscard]] AeadKey hkdf_journal_key(Span<const std::uint8_t> vmk, std::uint64_t sequence) noexcept;

} // namespace sv::crypto