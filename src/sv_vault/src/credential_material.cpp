#include <sv/vault/credential_material.hpp>

#include <sv/keyfile/combiner.hpp>

namespace sv::vault {

Result<std::array<std::uint8_t, 32>> derive_kdf_secret(const CredentialMaterial& cred) noexcept {
    std::vector<keyfile::KeyfileHash> hashes;
    hashes.reserve(cred.keyfiles.size());
    for (const auto& entry : cred.keyfiles) {
        hashes.push_back(entry.hash);
    }
    return keyfile::combine_credentials(cred.password_utf8, hashes);
}

} 