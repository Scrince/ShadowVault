#pragma once

#include <sv/crypto/crypto_provider.hpp>
#include <sv/result.hpp>
#include <sv/span.hpp>

#include <cstdint>

namespace sv::crypto {

/// Argon2id KDF dispatch. Returns NotImplemented when argon2 was not fetched.
class KdfEngine {
public:
    [[nodiscard]] static Result<void> argon2id(
        Span<const std::uint8_t> password,
        Span<const std::uint8_t> salt,
        KdfParams params,
        Span<std::uint8_t> derived_key_out) noexcept;

    [[nodiscard]] static bool is_available() noexcept;
};

} // namespace sv::crypto