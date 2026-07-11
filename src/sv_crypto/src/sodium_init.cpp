#include "sodium_init.hpp"

namespace sv::crypto::detail {

bool ensure_sodium_initialized() noexcept {
    static const bool initialized = []() noexcept -> bool {
        try {
            return sodium_init() >= 0;
        } catch (...) {
            return false;
        }
    }();
    return initialized;
}

} // namespace sv::crypto::detail