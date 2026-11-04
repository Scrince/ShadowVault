#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>
#include <sv/platform/terminal_secret.hpp>

#include <sstream>

namespace sv::cli {

int unlock_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("unlock")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    const auto path = resolve_vault_path(ctx, args, 0);
    if (path.empty()) {
        return output.error(ErrorCode::UsageError, "usage: sv unlock PATH");
    }

    bool hidden = false;
    bool protect_hidden = false;
    bool read_only = false;
    bool replace_session = ctx.globals.replace_session;
    std::vector<std::filesystem::path> keyfiles;
    CredentialReadOptions cred_options {};
    std::optional<std::uint32_t> kdf_m_cost;
    std::optional<std::uint32_t> kdf_t_cost;
    std::optional<std::uint32_t> kdf_p_cost;

    for (std::size_t i = (ctx.globals.vault_path.has_value() ? 0 : 1); i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (parse_credential_arg(args, i, cred_options)) {
            continue;
        }
        if (arg == "--hidden") {
            hidden = true;
        } else if (arg == "--protect-hidden") {
            protect_hidden = true;
        } else if (arg == "--read-only") {
            read_only = true;
        } else if (arg == "--replace-session") {
            replace_session = true;
        } else if (arg == "--keyfile" && i + 1 < args.size()) {
            keyfiles.emplace_back(args[++i]);
        } else if (arg == "--kdf-m-cost" || arg.rfind("--kdf-m-cost=", 0) == 0) {
            const auto value = arg == "--kdf-m-cost"
                ? (i + 1 < args.size() ? args[++i] : std::string {})
                : arg.substr(13);
            const auto parsed = parse_kdf_cost_value(value, 8192, 1048576);
            if (!parsed.has_value()) {
                return output.error(ErrorCode::UsageError, "--kdf-m-cost must be an integer from 8192 to 1048576");
            }
            kdf_m_cost = *parsed;
        } else if (arg == "--kdf-t-cost" || arg.rfind("--kdf-t-cost=", 0) == 0) {
            const auto value = arg == "--kdf-t-cost"
                ? (i + 1 < args.size() ? args[++i] : std::string {})
                : arg.substr(13);
            const auto parsed = parse_kdf_cost_value(value, 1, 10);
            if (!parsed.has_value()) {
                return output.error(ErrorCode::UsageError, "--kdf-t-cost must be an integer from 1 to 10");
            }
            kdf_t_cost = *parsed;
        } else if (arg == "--kdf-p-cost" || arg.rfind("--kdf-p-cost=", 0) == 0) {
            const auto value = arg == "--kdf-p-cost"
                ? (i + 1 < args.size() ? args[++i] : std::string {})
                : arg.substr(13);
            const auto parsed = parse_kdf_cost_value(value, 1, 4);
            if (!parsed.has_value()) {
                return output.error(ErrorCode::UsageError, "--kdf-p-cost must be an integer from 1 to 4");
            }
            kdf_p_cost = *parsed;
        }
    }

    auto cred_result = read_credentials(ctx.platform, keyfiles, cred_options);
    if (!cred_result.is_ok()) {
        return output.error(cred_result.error(), cred_result.message());
    }
    auto cred = std::move(cred_result.value());

    vault::OpenVaultOptions options {};
    options.intent = hidden ? vault::HeaderSlotIntent::Hidden : vault::HeaderSlotIntent::Outer;
    options.protect_hidden = protect_hidden;
    options.read_only = read_only;
    if (kdf_m_cost.has_value() || kdf_t_cost.has_value() || kdf_p_cost.has_value()) {
        options.kdf_params_override = true;
        if (kdf_m_cost.has_value()) {
            options.kdf_params.m_cost_kib = *kdf_m_cost;
        }
        if (kdf_t_cost.has_value()) {
            options.kdf_params.t_cost = *kdf_t_cost;
        }
        if (kdf_p_cost.has_value()) {
            options.kdf_params.p_cost = *kdf_p_cost;
        }
    }

    auto session = vault_engine().open(path, cred, options);
    platform::secure_wipe(cred.password_utf8);
    if (!session.is_ok()) {
        return output.error(session.error(), session.message());
    }

    if (const auto inserted = vault::SessionStore::instance().insert(std::move(session.value()), replace_session);
        !inserted.is_ok()) {
        return output.error(inserted.error(), inserted.message());
    }

    if (output.json_enabled()) {
        std::ostringstream json;
        json << "{\n";
        json << "  \"status\": \"unlocked\",\n";
        json << "  \"path\": " << json_path(path) << ",\n";
        json << "  \"hidden\": " << (hidden ? "true" : "false") << "\n";
        json << '}';
        return output.emit_json(json.str());
    }

    return output.success("unlocked vault: " + path.generic_string());
}

} 
