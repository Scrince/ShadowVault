#include <sv/platform/memory_lock.hpp>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace sv::platform {

Result<void> lock_memory(Span<std::byte> region) noexcept {
    if (region.empty()) {
        return Result<void>::ok();
    }

#if defined(_WIN32)
    if (VirtualLock(region.data(), region.size()) == 0) {
        return Result<void>::err(ErrorCode::PlatformError, "VirtualLock failed");
    }
#else
    if (mlock(region.data(), region.size()) != 0) {
        return Result<void>::err(ErrorCode::PlatformError, "mlock failed");
    }
#endif
    return Result<void>::ok();
}

Result<void> unlock_memory(Span<std::byte> region) noexcept {
    if (region.empty()) {
        return Result<void>::ok();
    }

#if defined(_WIN32)
    if (VirtualUnlock(region.data(), region.size()) == 0) {
        return Result<void>::err(ErrorCode::PlatformError, "VirtualUnlock failed");
    }
#else
    if (munlock(region.data(), region.size()) != 0) {
        return Result<void>::err(ErrorCode::PlatformError, "munlock failed");
    }
#endif
    return Result<void>::ok();
}

} // namespace sv::platform