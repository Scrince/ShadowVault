#pragma once

#include <sv/container/types.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/crypto/secure_buffer.hpp>
#include <sv/result.hpp>
#include <sv/vault/credential_material.hpp>

namespace sv::vault {

[[nodiscard]] crypto::KdfParams default_kdf_params_desktop() noexcept;
[[nodiscard]] crypto::KdfParams default_kdf_params_portable() noexcept;

enum class HeaderSlotIntent {
    Outer,
    Hidden,
};

struct DerivedKeys {
    crypto::SecureBuffer header_key;
};

class KeyManager {
public:
    explicit KeyManager(crypto::CryptoProvider& crypto) noexcept;

    [[nodiscard]] Result<DerivedKeys> derive_from_credentials(
        const CredentialMaterial& cred,
        const container::Superblock& sb,
        HeaderSlotIntent intent,
        crypto::KdfParams params = default_kdf_params_desktop()) noexcept;

    [[nodiscard]] Result<container::EncryptedHeaderSector> seal_header(
        const container::HeaderPayload& payload,
        const DerivedKeys& keys,
        const container::Superblock& sb,
        container::HeaderSlotId slot_id,
        container::HeaderSlotType slot_type) noexcept;

    [[nodiscard]] Result<container::HeaderPayload> open_header(
        const container::EncryptedHeaderSector& enc,
        const DerivedKeys& keys,
        const container::Superblock& sb,
        container::HeaderSlotId slot_id,
        container::HeaderSlotType slot_type) noexcept;

    [[nodiscard]] Result<std::array<std::uint8_t, 32>> compute_header_key_confirm(
        Span<const std::uint8_t> header_key) noexcept;

private:
    crypto::CryptoProvider& crypto_;
};

[[nodiscard]] crypto::KdfParams kdf_params_from_blob(Span<const std::uint8_t> blob) noexcept;
[[nodiscard]] void encode_kdf_params_blob(crypto::KdfParams params, Span<std::uint8_t> blob) noexcept;
[[nodiscard]] crypto::KdfParams outer_kdf_params_from_superblock(const container::Superblock& sb) noexcept;
[[nodiscard]] void store_outer_kdf_hint(container::Superblock& sb, crypto::KdfParams params) noexcept;

} 