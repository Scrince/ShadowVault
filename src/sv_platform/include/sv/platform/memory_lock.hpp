#pragma once

#include <sv/result.hpp>
#include <sv/span.hpp>

#include <cstddef>

namespace sv::platform {

/// Lock sensitive pages in RAM (VirtualLock / mlock). Best-effort; may fail on
/// systems with low lock quotas.
[[nodiscard]] Result<void> lock_memory(Span<std::byte> region) noexcept;

/// Release a prior lock acquired through lock_memory.
[[nodiscard]] Result<void> unlock_memory(Span<std::byte> region) noexcept;

} // namespace sv::platform