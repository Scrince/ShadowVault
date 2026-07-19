#pragma once

#include <sv/result.hpp>
#include <sv/span.hpp>

#include <cstdint>

namespace sv::crypto {


[[nodiscard]] Result<void> secure_random_bytes(Span<std::uint8_t> out) noexcept;

} 