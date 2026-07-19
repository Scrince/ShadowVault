#pragma once

#include <sodium.h>

namespace sv::crypto::detail {


[[nodiscard]] bool ensure_sodium_initialized() noexcept;

} 