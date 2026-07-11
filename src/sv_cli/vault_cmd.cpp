#include <sv/cli/vault_cmd.hpp>

#include <sv/keyfile/keyfile_format.hpp>
#include <sv/platform/terminal_secret.hpp>
#include <sv/vault/key_manager.hpp>
#include <sv/vault/vault_engine.hpp>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace sv::cli {

namespace {

vault::VaultEngine g_engine;

} // namespace

vault::VaultEngine& vault_engine() {
    return g_engine;
}

std::optional<std::uint64_t> parse_size_string(const std::string& text) {
    if (text.empty()) {
        return std::nullopt;
    }

    std::uint64_t multiplier = 1;
    std::string number = text;
    const char suffix = static_cast<char>(std::tolower(text.back()));
    if (suffix == 'k' || suffix == 'm' || suffix == 'g' || suffix == 't') {
        number = text.substr(0, text.size() - 1);
        switch (suffix) {
        case 'k':
            multiplier = 1024ULL;
            break;
        case 'm':
            multiplier = 1024ULL * 1024;
            break;
        case 'g':
            multiplier = 1024ULL * 1024 * 1024;
            break;
        case 't':
            multiplier = 1024ULL * 1024 * 1024 * 1024;
            break;
        default:
            break;
        }
    }

    try {
        const auto value = std::stoull(number);
        return value * multiplier;
    } catch (...) {
        return std::nullopt;
    }
}

namespace {

[[nodiscard]] bool parse_eq_value(
    const std::string& arg,
    const std::string& prefix,
    std::string& out) {
    if (arg.rfind(prefix, 0) != 0) {
        return false;
    }
    out = arg.substr(prefix.size());
    return true;
}

[[nodiscard]] Result<std::string> read_password_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<std::string>::err(ErrorCode::IoError, "failed to open password file");
    }
    std::string password;
    std::getline(input, password);
    if (password.empty()) {
        return Result<std::string>::err(ErrorCode::InvalidArgument, "password file is empty");
    }
    if (!password.empty() && password.back() == '\r') {
        password.pop_back();
    }
    return Result<std::string>::ok(std::move(password));
}

[[nodiscard]] Result<std::string> resolve_password(
    platform::PlatformContext& platform,
    const CredentialReadOptions& options) {
    if (options.inline_password.has_value()) {
        return Result<std::string>::ok(options.inline_password.value());
    }
    if (options.password_file.has_value()) {
        std::cerr << "warning: reading password from file is insecure\n";
        return read_password_file(*options.password_file);
    }
    if (options.allow_env_password) {
        if (const char* env = std::getenv("SV_PASSWORD"); env != nullptr && env[0] != '\0') {
            std::cerr << "warning: using SV_PASSWORD from environment (testing only)\n";
            return Result<std::string>::ok(std::string(env));
        }
    }

    auto password = platform::read_secret(options.prompt_label, true);
    if (!password.is_ok()) {
        return Result<std::string>::err(password.error(), password.message());
    }
    std::string value = std::move(password.value()).release();
    if (options.confirm_password) {
        auto confirm = platform::read_secret("Confirm password: ", true);
        if (!confirm.is_ok()) {
            platform::secure_wipe(value);
            return Result<std::string>::err(confirm.error(), confirm.message());
        }
        std::string confirm_value = std::move(confirm.value()).release();
        if (confirm_value != value) {
            platform::secure_wipe(value);
            platform::secure_wipe(confirm_value);
            return Result<std::string>::err(ErrorCode::InvalidArgument, "passwords do not match");
        }
        platform::secure_wipe(confirm_value);
    }
    return Result<std::string>::ok(std::move(value));
}

} // namespace

bool parse_credential_arg(
    const std::vector<std::string>& args,
    std::size_t& i,
    CredentialReadOptions& options) {
    const std::string& arg = args[i];
    std::string value;
    if (arg == "--password-file" && i + 1 < args.size()) {
        options.password_file = args[++i];
        return true;
    }
    if (parse_eq_value(arg, "--password-file=", value)) {
        options.password_file = value;
        return true;
    }
    if (arg == "--password" && i + 1 < args.size()) {
        options.inline_password = args[++i];
        return true;
    }
    if (parse_eq_value(arg, "--password=", value)) {
        options.inline_password = value;
        return true;
    }
    if (arg == "--allow-env-password") {
        options.allow_env_password = true;
        return true;
    }
    if (arg == "--password-prompt") {
        return true;
    }
    if (arg == "--new-password" && i + 1 < args.size()) {
        options.inline_password = args[++i];
        return true;
    }
    if (parse_eq_value(arg, "--new-password=", value)) {
        options.inline_password = value;
        return true;
    }
    return false;
}

std::vector<std::string> collect_positional_args(const std::vector<std::string>& args) {
    std::vector<std::string> positional;
    positional.reserve(args.size());

    for (std::size_t i = 0; i < args.size(); ++i) {
        std::size_t cursor = i;
        CredentialReadOptions cred {};
        if (parse_credential_arg(args, cursor, cred)) {
            i = cursor;
            continue;
        }

        const std::string& arg = args[i];
        if (arg == "--keyfile" && i + 1 < args.size()) {
            ++i;
            continue;
        }
        if (arg == "--force" || arg == "--replace-session") {
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            continue;
        }
        positional.push_back(arg);
    }

    return positional;
}

void apply_kdf_profile(const std::string& profile, vault::CreateVaultOptions& options) {
    if (profile == "portable") {
        options.kdf_params = vault::default_kdf_params_portable();
        return;
    }
    if (profile == "paranoid") {
        options.kdf_params = vault::default_kdf_params_desktop();
        options.kdf_params.m_cost_kib = 524288;
        options.kdf_params.t_cost = 4;
        return;
    }
    options.kdf_params = vault::default_kdf_params_desktop();
}

Result<vault::CredentialMaterial> read_credentials(
    platform::PlatformContext& platform,
    const std::vector<std::filesystem::path>& keyfile_paths,
    const CredentialReadOptions& options) {
    const auto password = resolve_password(platform, options);
    if (!password.is_ok()) {
        return Result<vault::CredentialMaterial>::err(password.error(), password.message());
    }

    vault::CredentialMaterial cred {};
    cred.password_utf8 = password.value();

    const auto keyfiles = keyfile::read_keyfiles(keyfile_paths);
    if (!keyfiles.is_ok()) {
        platform::secure_wipe(cred.password_utf8);
        return Result<vault::CredentialMaterial>::err(keyfiles.error(), keyfiles.message());
    }
    cred.keyfiles = std::move(keyfiles.value());
    return Result<vault::CredentialMaterial>::ok(std::move(cred));
}

std::filesystem::path resolve_vault_path(
    const CliContext& ctx,
    const std::vector<std::string>& args,
    const std::size_t index) {
    if (ctx.globals.vault_path.has_value()) {
        return *ctx.globals.vault_path;
    }
    if (index < args.size()) {
        return std::filesystem::path(args[index]);
    }
    return {};
}

Result<vault::UnlockedVault*> ensure_vault_session(
    CliContext& ctx,
    const std::filesystem::path& container_path,
    const std::vector<std::string>& args,
    const SessionResolveOptions options) {
    if (!container_path.empty()) {
        const auto existing = vault::SessionStore::instance().find(container_path);
        if (existing.is_ok()) {
            return existing;
        }
    } else {
        const auto any = vault::SessionStore::instance().find_any();
        if (any.is_ok()) {
            return any;
        }
    }

    if (container_path.empty()) {
        return Result<vault::UnlockedVault*>::err(
            ErrorCode::VaultLocked,
            "no active vault session (use --vault PATH with --password, or unlock first)");
    }

    CredentialReadOptions cred_options {};
    cred_options.prompt_label = "Password: ";
    std::vector<std::filesystem::path> keyfiles;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--new-password" || arg.rfind("--new-password=", 0) == 0) {
            if (arg == "--new-password" && i + 1 < args.size()) {
                ++i;
            }
            continue;
        }
        if (parse_credential_arg(args, i, cred_options)) {
            continue;
        }
        if (arg == "--keyfile" && i + 1 < args.size()) {
            keyfiles.emplace_back(args[++i]);
        }
    }

    const bool has_inline_cred = cred_options.inline_password.has_value()
        || cred_options.password_file.has_value()
        || cred_options.allow_env_password;

    if (!has_inline_cred) {
        return Result<vault::UnlockedVault*>::err(
            ErrorCode::VaultLocked,
            "vault is locked (pass --password or unlock in the same process)");
    }

    auto cred_result = read_credentials(ctx.platform, keyfiles, cred_options);
    if (!cred_result.is_ok()) {
        return Result<vault::UnlockedVault*>::err(cred_result.error(), cred_result.message());
    }
    auto cred = std::move(cred_result.value());

    vault::OpenVaultOptions open_opts {};
    open_opts.intent = options.hidden ? vault::HeaderSlotIntent::Hidden : vault::HeaderSlotIntent::Outer;
    open_opts.protect_hidden = options.protect_hidden;
    open_opts.read_only = options.read_only;

    auto opened = vault_engine().open(container_path, cred, open_opts);
    platform::secure_wipe(cred.password_utf8);
    if (!opened.is_ok()) {
        return Result<vault::UnlockedVault*>::err(opened.error(), opened.message());
    }

    if (const auto inserted = vault::SessionStore::instance().insert(
            std::move(opened.value()),
            options.replace_session);
        !inserted.is_ok()) {
        return Result<vault::UnlockedVault*>::err(inserted.error(), inserted.message());
    }

    return vault::SessionStore::instance().find(container_path);
}

} // namespace sv::cli