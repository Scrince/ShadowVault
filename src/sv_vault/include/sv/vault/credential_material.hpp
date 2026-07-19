#pragma once

#include <sv/keyfile/keyfile_format.hpp>
#include <sv/result.hpp>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace sv::vault {

struct CredentialMaterial {
    std::string password_utf8;
    std::vector<keyfile::KeyfileEntry> keyfiles;
};

[[nodiscard]] Result<std::array<std::uint8_t, 32>> derive_kdf_secret(const CredentialMaterial& cred) noexcept;

} 