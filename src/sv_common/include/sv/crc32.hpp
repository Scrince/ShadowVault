#pragma once

#include <sv/span.hpp>

#include <cstddef>
#include <cstdint>

namespace sv {


[[nodiscard]] std::uint32_t crc32_ieee(Span<const std::uint8_t> data) noexcept;

} 