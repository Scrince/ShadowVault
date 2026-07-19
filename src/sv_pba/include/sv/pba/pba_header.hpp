#pragma once

#include <sv/result.hpp>
#include <sv/span.hpp>

#include <cstdint>

namespace sv::pba {


struct SystemHeaderView {
    bool magic_ok = false;
    std::uint16_t format_version = 0;
};


[[nodiscard]] SystemHeaderView inspect_header_sector(Span<const std::uint8_t> sector) noexcept;

} 