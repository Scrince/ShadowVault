#include <sv/keyfile/combiner.hpp>

#include <sodium.h>

#include <cstring>

namespace sv::keyfile {

namespace {

constexpr std::size_t kKdfSecretSize = 32;

} 

Result<std::array<std::uint8_t, 32>> combine_credentials(
    const std::string_view password_utf8,
    const Span<const KeyfileHash> keyfile_hashes) noexcept {
    if (password_utf8.empty()) {
        return Result<std::array<std::uint8_t, 32>>::err(ErrorCode::InvalidArgument, "password must not be empty");
    }
    if (password_utf8.size() < 12) {
        return Result<std::array<std::uint8_t, 32>>::err(
            ErrorCode::InvalidArgument,
            "password must be at least 12 characters");
    }

    std::array<std::uint8_t, kKdfSecretSize> step {};
    if (crypto_hash_sha256(
            step.data(),
            reinterpret_cast<const unsigned char*>(password_utf8.data()),
            password_utf8.size())
        != 0) {
        return Result<std::array<std::uint8_t, 32>>::err(ErrorCode::PlatformError, "sha256 failed");
    }

    for (const auto& keyfile_hash : keyfile_hashes) {
        std::array<std::uint8_t, kKdfSecretSize> next {};
        if (crypto_auth_hmacsha256(
                next.data(),
                keyfile_hash.data(),
                keyfile_hash.size(),
                step.data())
            != 0) {
            sodium_memzero(step.data(), step.size());
            return Result<std::array<std::uint8_t, 32>>::err(ErrorCode::PlatformError, "hmac-sha256 failed");
        }
        step = next;
    }

    return Result<std::array<std::uint8_t, 32>>::ok(step);
}

} 