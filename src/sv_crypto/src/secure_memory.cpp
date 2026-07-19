#include "secure_memory_detail.hpp"
#include "sodium_init.hpp"

#include <sv/platform/memory_lock.hpp>

#include <cstring>

namespace sv::crypto::detail {

Result<void> lock_secure_region(Span<std::byte> region) noexcept {
    return platform::lock_memory(region);
}

Result<void> unlock_secure_region(Span<std::byte> region) noexcept {
    return platform::unlock_memory(region);
}

void wipe_secure_region(Span<std::uint8_t> region) noexcept {
    if (region.empty()) {
        return;
    }
    if (ensure_sodium_initialized()) {
        sodium_memzero(region.data(), region.size());
    } else {
        std::memset(region.data(), 0, region.size());
    }
}

} 