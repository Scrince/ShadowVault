#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>
#include <sv/keyfile/keyfile_format.hpp>
#include <sv/platform/terminal_secret.hpp>

namespace sv::cli {

int add_keyfile_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("add-keyfile")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    const auto path = resolve_vault_path(ctx, args, 0);
    if (path.empty()) {
        return output.error(ErrorCode::UsageError, "usage: sv add-keyfile PATH --keyfile KPATH");
    }

    std::vector<std::filesystem::path> existing_keyfiles;
    std::optional<std::filesystem::path> new_keyfile_path;
    CredentialReadOptions cred_options {};
    cred_options.prompt_label = "Password: ";

    for (std::size_t i = (ctx.globals.vault_path.has_value() ? 0 : 1); i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--keyfile" && i + 1 < args.size()) {
            new_keyfile_path = std::filesystem::path(args[++i]);
            continue;
        }
        if (arg.rfind("--keyfile=", 0) == 0) {
            new_keyfile_path = std::filesystem::path(arg.substr(10));
            continue;
        }
        if (arg == "--existing-keyfile" && i + 1 < args.size()) {
            existing_keyfiles.emplace_back(args[++i]);
            continue;
        }
        if (parse_credential_arg(args, i, cred_options)) {
            continue;
        }
    }

    if (!new_keyfile_path.has_value()) {
        return output.error(ErrorCode::UsageError, "add-keyfile requires --keyfile KPATH");
    }

    auto old_result = read_credentials(ctx.platform, existing_keyfiles, cred_options);
    if (!old_result.is_ok()) {
        return output.error(old_result.error(), old_result.message());
    }
    auto old_cred = std::move(old_result.value());

    const auto new_keyfile = keyfile::read_keyfile(*new_keyfile_path);
    if (!new_keyfile.is_ok()) {
        platform::secure_wipe(old_cred.password_utf8);
        return output.error(new_keyfile.error(), new_keyfile.message());
    }

    const auto added = vault_engine().add_keyfile(path, old_cred, new_keyfile.value());
    platform::secure_wipe(old_cred.password_utf8);
    if (!added.is_ok()) {
        return output.error(added.error(), added.message());
    }

    return output.success("keyfile added");
}

int remove_keyfile_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("remove-keyfile")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    const auto path = resolve_vault_path(ctx, args, 0);
    if (path.empty()) {
        return output.error(ErrorCode::UsageError, "usage: sv remove-keyfile PATH --keyfile KPATH");
    }

    std::vector<std::filesystem::path> unlock_keyfiles;
    std::optional<std::filesystem::path> remove_keyfile_path;
    CredentialReadOptions cred_options {};
    cred_options.prompt_label = "Password: ";

    for (std::size_t i = (ctx.globals.vault_path.has_value() ? 0 : 1); i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--keyfile" && i + 1 < args.size()) {
            remove_keyfile_path = std::filesystem::path(args[++i]);
            continue;
        }
        if (arg.rfind("--keyfile=", 0) == 0) {
            remove_keyfile_path = std::filesystem::path(arg.substr(10));
            continue;
        }
        if (arg == "--existing-keyfile" && i + 1 < args.size()) {
            unlock_keyfiles.emplace_back(args[++i]);
            continue;
        }
        if (parse_credential_arg(args, i, cred_options)) {
            continue;
        }
    }

    if (!remove_keyfile_path.has_value()) {
        return output.error(ErrorCode::UsageError, "remove-keyfile requires --keyfile KPATH");
    }

    unlock_keyfiles.push_back(*remove_keyfile_path);
    auto cred_result = read_credentials(ctx.platform, unlock_keyfiles, cred_options);
    if (!cred_result.is_ok()) {
        return output.error(cred_result.error(), cred_result.message());
    }
    auto cred = std::move(cred_result.value());

    const auto remove_entry = keyfile::read_keyfile(*remove_keyfile_path);
    if (!remove_entry.is_ok()) {
        platform::secure_wipe(cred.password_utf8);
        return output.error(remove_entry.error(), remove_entry.message());
    }
    const auto removed_hash = remove_entry.value().hash;
    const auto removed = vault_engine().remove_keyfile(path, cred, removed_hash);
    platform::secure_wipe(cred.password_utf8);
    if (!removed.is_ok()) {
        return output.error(removed.error(), removed.message());
    }

    return output.success("keyfile removed");
}

} // namespace sv::cli