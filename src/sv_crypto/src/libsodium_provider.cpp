#include <sv/crypto/constant_time.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/crypto/kdf_engine.hpp>
#include <sv/crypto/secure_random.hpp>

#include "sodium_init.hpp"

#include <cstring>
#include <vector>

namespace sv::crypto {

namespace {

class LibsodiumCryptoProvider final : public CryptoProvider {
public:
    LibsodiumCryptoProvider() noexcept = default;

    Result<void> aead_encrypt(
        Span<const std::uint8_t> plaintext,
        Span<const std::uint8_t> associated_data,
        const AeadKey& key,
        const AeadNonce& nonce,
        Span<std::uint8_t> ciphertext_out,
        Span<std::uint8_t> tag_out) override {
        if (!detail::ensure_sodium_initialized()) {
            return Result<void>::err(ErrorCode::PlatformError, "libsodium initialization failed");
        }
        if (ciphertext_out.size() != plaintext.size()) {
            return Result<void>::err(ErrorCode::InvalidArgument, "ciphertext buffer size mismatch");
        }
        if (tag_out.size() != kAeadTagSize) {
            return Result<void>::err(ErrorCode::InvalidArgument, "tag buffer must be 16 bytes");
        }

        unsigned long long tag_len = 0;
        if (crypto_aead_xchacha20poly1305_ietf_encrypt_detached(
                ciphertext_out.data(),
                tag_out.data(),
                &tag_len,
                plaintext.data(),
                plaintext.size(),
                associated_data.data(),
                associated_data.size(),
                nullptr,
                nonce.bytes.data(),
                key.bytes.data()) != 0) {
            return Result<void>::err(ErrorCode::PlatformError, "aead encrypt failed");
        }
        if (tag_len != kAeadTagSize) {
            return Result<void>::err(ErrorCode::PlatformError, "unexpected aead tag length");
        }
        return Result<void>::ok();
    }

    Result<void> aead_decrypt(
        Span<const std::uint8_t> ciphertext,
        Span<const std::uint8_t> associated_data,
        const AeadKey& key,
        const AeadNonce& nonce,
        Span<const std::uint8_t> tag,
        Span<std::uint8_t> plaintext_out) override {
        if (!detail::ensure_sodium_initialized()) {
            return Result<void>::err(ErrorCode::PlatformError, "libsodium initialization failed");
        }
        if (plaintext_out.size() != ciphertext.size()) {
            return Result<void>::err(ErrorCode::InvalidArgument, "plaintext buffer size mismatch");
        }
        if (tag.size() != kAeadTagSize) {
            return Result<void>::err(ErrorCode::InvalidArgument, "tag must be 16 bytes");
        }

        if (crypto_aead_xchacha20poly1305_ietf_decrypt_detached(
                plaintext_out.data(),
                nullptr,
                ciphertext.data(),
                ciphertext.size(),
                tag.data(),
                associated_data.data(),
                associated_data.size(),
                nonce.bytes.data(),
                key.bytes.data()) != 0) {
            sodium_memzero(plaintext_out.data(), plaintext_out.size());
            return Result<void>::err(ErrorCode::WrongCredentials, "aead decrypt failed");
        }
        return Result<void>::ok();
    }

    Result<void> kdf_argon2id(
        Span<const std::uint8_t> password,
        Span<const std::uint8_t> salt,
        KdfParams params,
        Span<std::uint8_t> derived_key_out) override {
        return KdfEngine::argon2id(password, salt, params, derived_key_out);
    }

    void secure_zero(Span<std::uint8_t> buffer) override {
        if (buffer.empty()) {
            return;
        }
        if (detail::ensure_sodium_initialized()) {
            sodium_memzero(buffer.data(), buffer.size());
        }
    }

    Result<void> random_bytes(Span<std::uint8_t> out) override {
        return secure_random_bytes(out);
    }

    bool constant_time_eq(Span<const std::uint8_t> a, Span<const std::uint8_t> b) override {
        return constant_time_compare(a, b);
    }
};

} // namespace

std::unique_ptr<CryptoProvider> create_default_crypto_provider() noexcept {
    try {
        if (!detail::ensure_sodium_initialized()) {
            return nullptr;
        }
        return std::make_unique<LibsodiumCryptoProvider>();
    } catch (...) {
        return nullptr;
    }
}

} // namespace sv::crypto