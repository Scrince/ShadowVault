#include <sv/crc32.hpp>

namespace sv {

namespace {

constexpr std::uint32_t kCrc32Polynomial = 0xEDB88320U;

[[nodiscard]] std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t byte) noexcept {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
        const std::uint32_t mask = static_cast<std::uint32_t>(-(static_cast<std::int32_t>(crc & 1U)));
        crc = (crc >> 1) ^ (kCrc32Polynomial & mask);
    }
    return crc;
}

} // namespace

std::uint32_t crc32_ieee(Span<const std::uint8_t> data) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::uint8_t byte : data) {
        crc = crc32_update(crc, byte);
    }
    return crc ^ 0xFFFFFFFFU;
}

} // namespace sv