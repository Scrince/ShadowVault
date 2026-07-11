#include <sv/crypto/secure_random.hpp>

#include "sodium_init.hpp"

namespace sv::crypto {

Result<void> secure_random_bytes(Span<std::uint8_t> out) noexcept {
    if (!detail::ensure_sodium_initialized()) {
        return Result<void>::err(ErrorCode::PlatformError, "libsodium initialization failed");
    }
    if (out.empty()) {
        return Result<void>::ok();
    }
    randombytes_buf(out.data(), out.size());
    return Result<void>::ok();
}

} // namespace sv::crypto