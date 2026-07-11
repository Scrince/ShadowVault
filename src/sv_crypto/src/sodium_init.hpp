#pragma once

#include <sodium.h>

namespace sv::crypto::detail {

/// Initialize libsodium exactly once. Never throws.
[[nodiscard]] bool ensure_sodium_initialized() noexcept;

} // namespace sv::crypto::detail