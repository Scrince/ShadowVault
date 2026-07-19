#include <sv/crypto/constant_time.hpp>

#include "sodium_init.hpp"

namespace sv::crypto {

bool constant_time_compare(
    Span<const std::uint8_t> left,
    Span<const std::uint8_t> right) noexcept {
    if (!detail::ensure_sodium_initialized()) {
        return false;
    }
    if (left.size() != right.size()) {
        return false;
    }
    if (left.empty()) {
        return true;
    }
    return sodium_memcmp(left.data(), right.data(), left.size()) == 0;
}

} 