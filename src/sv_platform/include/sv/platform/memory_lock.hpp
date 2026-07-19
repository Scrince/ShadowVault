#pragma once

#include <sv/result.hpp>
#include <sv/span.hpp>

#include <cstddef>

namespace sv::platform {


[[nodiscard]] Result<void> lock_memory(Span<std::byte> region) noexcept;


[[nodiscard]] Result<void> unlock_memory(Span<std::byte> region) noexcept;

} 