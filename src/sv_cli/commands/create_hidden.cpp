#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>
#include <sv/platform/terminal_secret.hpp>
#include <sv/vault/hidden_volume.hpp>
#include <sv/vault/session_store.hpp>

namespace sv::cli {

int create_hidden_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("create-hidden")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    if (args.empty()) {
        return output.error(ErrorCode::UsageError, "usage: sv create-hidden VAULT --size SIZE");
    }

    const std::filesystem::path path = args.front();
    std::optional<std::uint64_t> hidden_size;

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--size" && i + 1 < args.size()) {
            hidden_size = parse_size_string(args[++i]);
        } else if (arg.rfind("--size=", 0) == 0) {
            hidden_size = parse_size_string(arg.substr(7));
        }
    }

    if (!hidden_size.has_value()) {
        return output.error(ErrorCode::UsageError, "--size is required");
    }

    auto outer_session = ensure_vault_session(ctx, path, args);
    if (!outer_session.is_ok()) {
        return output.error(outer_session.error(), outer_session.message());
    }

    CredentialReadOptions hidden_options {};
    hidden_options.prompt_label = "Hidden password: ";
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--new-password" && i + 1 < args.size()) {
            hidden_options.inline_password = args[++i];
            break;
        }
        if (args[i].rfind("--new-password=", 0) == 0) {
            hidden_options.inline_password = args[i].substr(15);
            break;
        }
    }

    auto hidden_cred_result = read_credentials(ctx.platform, {}, hidden_options);
    if (!hidden_cred_result.is_ok()) {
        return output.error(hidden_cred_result.error(), hidden_cred_result.message());
    }
    auto hidden_cred = std::move(hidden_cred_result.value());

    const auto created = vault::create_hidden_volume(
        vault_engine(),
        *outer_session.value(),
        hidden_cred,
        *hidden_size);
    platform::secure_wipe(hidden_cred.password_utf8);
    if (!created.is_ok()) {
        return output.error(created.error(), created.message());
    }

    return output.success("hidden volume created");
}

} // namespace sv::cli