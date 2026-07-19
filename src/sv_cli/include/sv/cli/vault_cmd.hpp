#pragma once

#include <sv/cli/command_registry.hpp>
#include <sv/result.hpp>
#include <sv/vault/credential_material.hpp>
#include <sv/vault/session_store.hpp>
#include <sv/vault/vault_engine.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sv::cli {

[[nodiscard]] std::optional<std::uint64_t> parse_size_string(const std::string& text);
[[nodiscard]] void apply_kdf_profile(const std::string& profile, vault::CreateVaultOptions& options);

struct CredentialReadOptions {
    std::optional<std::filesystem::path> password_file;
    std::optional<std::string> inline_password;
    bool allow_env_password = false;
    bool confirm_password = false;
    std::string prompt_label = "Password: ";
};


[[nodiscard]] bool parse_credential_arg(
    const std::vector<std::string>& args,
    std::size_t& i,
    CredentialReadOptions& options);


[[nodiscard]] std::vector<std::string> collect_positional_args(const std::vector<std::string>& args);

[[nodiscard]] Result<vault::CredentialMaterial> read_credentials(
    platform::PlatformContext& platform,
    const std::vector<std::filesystem::path>& keyfile_paths,
    const CredentialReadOptions& options = {});

[[nodiscard]] std::filesystem::path resolve_vault_path(
    const CliContext& ctx,
    const std::vector<std::string>& args,
    std::size_t index = 0);

struct SessionResolveOptions {
    bool hidden = false;
    bool protect_hidden = false;
    bool read_only = false;
    bool replace_session = true;
};


[[nodiscard]] Result<vault::UnlockedVault*> ensure_vault_session(
    CliContext& ctx,
    const std::filesystem::path& container_path,
    const std::vector<std::string>& args,
    SessionResolveOptions options = {});

[[nodiscard]] vault::VaultEngine& vault_engine();

} 