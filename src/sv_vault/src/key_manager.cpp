#include <sv/vault/key_manager.hpp>

#include <sv/container/header_codec.hpp>
#include <sv/crypto/constant_time.hpp>
#include <sv/endian.hpp>

#include <sodium.h>

#include <algorithm>
#include <cstring>

namespace sv::vault {

namespace {

constexpr std::size_t kHeaderKeySize = 32;
constexpr std::size_t kKdfOutputSize = 64;
constexpr std::array<char, 14> kConfirmInfo = {'s', 'v', '-', 'c', 'o', 'n', 'f', 'i', 'r', 'm', '-', 'v', '1', '\0'};

} // namespace

KeyManager::KeyManager(crypto::CryptoProvider& crypto) noexcept : crypto_(crypto) {}

crypto::KdfParams default_kdf_params_desktop() noexcept {
    crypto::KdfParams params {};
    params.m_cost_kib = 262144;
    params.t_cost = 3;
    params.p_cost = 1;
    return params;
}

crypto::KdfParams default_kdf_params_portable() noexcept {
    crypto::KdfParams params {};
    params.m_cost_kib = 65536;
    params.t_cost = 2;
    params.p_cost = 1;
    return params;
}

crypto::KdfParams kdf_params_from_blob(Span<const std::uint8_t> blob) noexcept {
    crypto::KdfParams params {};
    if (blob.size() < 12) {
        return params;
    }
    params.m_cost_kib = read_le32(blob.data());
    params.t_cost = read_le32(blob.data() + 4);
    params.p_cost = read_le32(blob.data() + 8);
    params.parallelism_reserved = read_le32(blob.data() + 12);
    return params;
}

crypto::KdfParams outer_kdf_params_from_superblock(const container::Superblock& sb) noexcept {
    const bool has_hint = std::any_of(
        sb.outer_kdf_id.begin(),
        sb.outer_kdf_id.end(),
        [](const std::uint8_t byte) { return byte != 0; });
    if (!has_hint) {
        return default_kdf_params_desktop();
    }
    std::array<std::uint8_t, container::SVLT_KDF_PARAMS_BLOB_SIZE> blob {};
    std::memcpy(blob.data(), sb.outer_kdf_id.data(), sb.outer_kdf_id.size());
    return kdf_params_from_blob(blob);
}

void store_outer_kdf_hint(container::Superblock& sb, const crypto::KdfParams params) noexcept {
    std::array<std::uint8_t, container::SVLT_KDF_PARAMS_BLOB_SIZE> blob {};
    encode_kdf_params_blob(params, blob);
    std::memcpy(sb.outer_kdf_id.data(), blob.data(), sb.outer_kdf_id.size());
}

void encode_kdf_params_blob(const crypto::KdfParams params, Span<std::uint8_t> blob) noexcept {
    if (blob.size() < container::SVLT_KDF_PARAMS_BLOB_SIZE) {
        return;
    }
    std::memset(blob.data(), 0, blob.size());
    write_le32(blob.data(), params.m_cost_kib);
    write_le32(blob.data() + 4, params.t_cost);
    write_le32(blob.data() + 8, params.p_cost);
    write_le32(blob.data() + 12, params.parallelism_reserved);
}

Result<std::array<std::uint8_t, 32>> KeyManager::compute_header_key_confirm(
    const Span<const std::uint8_t> header_key) noexcept {
    if (header_key.size() != kHeaderKeySize) {
        return Result<std::array<std::uint8_t, 32>>::err(ErrorCode::InvalidArgument, "header_key must be 32 bytes");
    }

    std::array<std::uint8_t, 32> confirm {};
    if (crypto_auth_hmacsha256(
            confirm.data(),
            reinterpret_cast<const unsigned char*>(kConfirmInfo.data()),
            kConfirmInfo.size() - 1,
            header_key.data())
        != 0) {
        return Result<std::array<std::uint8_t, 32>>::err(ErrorCode::PlatformError, "hmac confirm failed");
    }
    return Result<std::array<std::uint8_t, 32>>::ok(confirm);
}

Result<DerivedKeys> KeyManager::derive_from_credentials(
    const CredentialMaterial& cred,
    const container::Superblock& sb,
    const HeaderSlotIntent intent,
    crypto::KdfParams params) noexcept {
    const auto kdf_secret = derive_kdf_secret(cred);
    if (!kdf_secret.is_ok()) {
        return Result<DerivedKeys>::err(kdf_secret.error(), kdf_secret.message());
    }

    Span<const std::uint8_t> salt = sb.outer_salt;
    if (intent == HeaderSlotIntent::Hidden) {
        salt = sb.hidden_salt;
    }

    std::array<std::uint8_t, kKdfOutputSize> derived {};
    auto secret = kdf_secret.value();
    const auto kdf = crypto_.kdf_argon2id(secret, salt, params, derived);
    crypto_.secure_zero(secret);
    if (!kdf.is_ok()) {
        crypto_.secure_zero(derived);
        return Result<DerivedKeys>::err(kdf.error(), kdf.message());
    }

    DerivedKeys keys {};
    keys.header_key.resize(kHeaderKeySize);
    std::memcpy(keys.header_key.data(), derived.data(), kHeaderKeySize);
    crypto_.secure_zero(derived);
    return Result<DerivedKeys>::ok(std::move(keys));
}

Result<container::EncryptedHeaderSector> KeyManager::seal_header(
    const container::HeaderPayload& payload,
    const DerivedKeys& keys,
    const container::Superblock& sb,
    const container::HeaderSlotId slot_id,
    const container::HeaderSlotType slot_type) noexcept {
    return container::seal_header_sector(
        crypto_,
        payload,
        keys.header_key.span(),
        sb,
        slot_id,
        slot_type);
}

Result<container::HeaderPayload> KeyManager::open_header(
    const container::EncryptedHeaderSector& enc,
    const DerivedKeys& keys,
    const container::Superblock& sb,
    const container::HeaderSlotId slot_id,
    const container::HeaderSlotType slot_type) noexcept {
    auto payload = container::open_header_sector(
        crypto_,
        enc,
        keys.header_key.span(),
        sb,
        slot_id,
        slot_type);
    if (!payload.is_ok()) {
        return payload;
    }

    const auto confirm = compute_header_key_confirm(keys.header_key.span());
    if (!confirm.is_ok()) {
        return Result<container::HeaderPayload>::err(confirm.error(), confirm.message());
    }

    if (!crypto_.constant_time_eq(payload.value().header_key_confirm, confirm.value())) {
        return Result<container::HeaderPayload>::err(ErrorCode::WrongCredentials, "header_key_confirm mismatch");
    }

    return payload;
}

} // namespace sv::vault