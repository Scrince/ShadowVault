#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>
#include <sv/platform/terminal_secret.hpp>

namespace sv::cli {

int change_password_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("change-password")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    const auto path = resolve_vault_path(ctx, args, 0);
    if (path.empty()) {
        return output.error(ErrorCode::UsageError, "usage: sv change-password PATH");
    }

    CredentialReadOptions old_options {};
    old_options.prompt_label = "Old password: ";
    CredentialReadOptions new_options {};
    new_options.prompt_label = "New password: ";
    new_options.confirm_password = true;

    for (std::size_t i = (ctx.globals.vault_path.has_value() ? 0 : 1); i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--new-password" && i + 1 < args.size()) {
            new_options.inline_password = args[++i];
            continue;
        }
        if (arg.rfind("--new-password=", 0) == 0) {
            new_options.inline_password = arg.substr(15);
            continue;
        }
        if (parse_credential_arg(args, i, old_options)) {
            continue;
        }
    }
    if (new_options.inline_password.has_value()) {
        new_options.confirm_password = false;
    }

    auto old_result = read_credentials(ctx.platform, {}, old_options);
    if (!old_result.is_ok()) {
        return output.error(old_result.error(), old_result.message());
    }
    auto old_cred = std::move(old_result.value());

    auto new_result = read_credentials(ctx.platform, {}, new_options);
    if (!new_result.is_ok()) {
        platform::secure_wipe(old_cred.password_utf8);
        return output.error(new_result.error(), new_result.message());
    }
    auto new_cred = std::move(new_result.value());

    const auto changed = vault_engine().change_password(path, old_cred, new_cred);
    platform::secure_wipe(old_cred.password_utf8);
    platform::secure_wipe(new_cred.password_utf8);
    if (!changed.is_ok()) {
        return output.error(changed.error(), changed.message());
    }

    return output.success("password changed");
}

} // namespace sv::cli