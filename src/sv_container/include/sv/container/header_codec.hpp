#pragma once

#include <sv/container/types.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/result.hpp>
#include <sv/span.hpp>

namespace sv::container {

[[nodiscard]] Result<std::array<std::uint8_t, SVLT_AAD_SIZE>> build_header_aad(
    const Superblock& sb,
    HeaderSlotId slot_id,
    HeaderSlotType slot_type) noexcept;

[[nodiscard]] Result<void> encode_header_payload(const HeaderPayload& payload, Span<std::uint8_t> out) noexcept;
[[nodiscard]] Result<HeaderPayload> decode_header_payload(Span<const std::uint8_t> data) noexcept;

[[nodiscard]] Result<void> encode_header_sector(const EncryptedHeaderSector& sector, Span<std::uint8_t> out) noexcept;
[[nodiscard]] Result<EncryptedHeaderSector> decode_header_sector(Span<const std::uint8_t> data) noexcept;

[[nodiscard]] Result<EncryptedHeaderSector> seal_header_sector(
    crypto::CryptoProvider& crypto,
    const HeaderPayload& payload,
    Span<const std::uint8_t> header_key,
    const Superblock& sb,
    HeaderSlotId slot_id,
    HeaderSlotType slot_type) noexcept;

[[nodiscard]] Result<HeaderPayload> open_header_sector(
    crypto::CryptoProvider& crypto,
    const EncryptedHeaderSector& sector,
    Span<const std::uint8_t> header_key,
    const Superblock& sb,
    HeaderSlotId slot_id,
    HeaderSlotType slot_type) noexcept;

} 