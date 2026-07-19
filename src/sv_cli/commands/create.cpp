#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>
#include <sv/platform/terminal_secret.hpp>

#include <climits>
#include <sstream>

namespace sv::cli {

int create_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("create")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    if (args.empty()) {
        return output.error(ErrorCode::UsageError, "usage: sv create PATH --size SIZE");
    }

    const std::filesystem::path path = args.front();
    std::optional<std::uint64_t> size;
    std::optional<std::uint32_t> chunk_size;
    std::string kdf_profile = "desktop";
    std::vector<std::filesystem::path> keyfiles;
    bool quick = false;
    CredentialReadOptions cred_options {};
    cred_options.confirm_password = true;
    std::optional<std::uint32_t> kdf_m_cost;
    std::optional<std::uint32_t> kdf_t_cost;
    std::optional<std::uint32_t> kdf_p_cost;

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (parse_credential_arg(args, i, cred_options)) {
            continue;
        }
        if (arg == "--size" && i + 1 < args.size()) {
            size = parse_size_string(args[++i]);
        } else if (arg.rfind("--size=", 0) == 0) {
            size = parse_size_string(arg.substr(7));
        } else if (arg == "--chunk-size" && i + 1 < args.size()) {
            const auto parsed = parse_size_string(args[++i]);
            if (parsed.has_value() && *parsed <= UINT32_MAX) {
                chunk_size = static_cast<std::uint32_t>(*parsed);
            }
        } else if (arg.rfind("--chunk-size=", 0) == 0) {
            const auto parsed = parse_size_string(arg.substr(13));
            if (parsed.has_value() && *parsed <= UINT32_MAX) {
                chunk_size = static_cast<std::uint32_t>(*parsed);
            }
        } else if (arg == "--kdf-profile" && i + 1 < args.size()) {
            kdf_profile = args[++i];
        } else if (arg.rfind("--kdf-profile=", 0) == 0) {
            kdf_profile = arg.substr(14);
        } else if (arg == "--kdf-m-cost" && i + 1 < args.size()) {
            kdf_m_cost = static_cast<std::uint32_t>(std::stoul(args[++i]));
        } else if (arg.rfind("--kdf-m-cost=", 0) == 0) {
            kdf_m_cost = static_cast<std::uint32_t>(std::stoul(arg.substr(13)));
        } else if (arg == "--kdf-t-cost" && i + 1 < args.size()) {
            kdf_t_cost = static_cast<std::uint32_t>(std::stoul(args[++i]));
        } else if (arg.rfind("--kdf-t-cost=", 0) == 0) {
            kdf_t_cost = static_cast<std::uint32_t>(std::stoul(arg.substr(13)));
        } else if (arg == "--kdf-p-cost" && i + 1 < args.size()) {
            kdf_p_cost = static_cast<std::uint32_t>(std::stoul(args[++i]));
        } else if (arg.rfind("--kdf-p-cost=", 0) == 0) {
            kdf_p_cost = static_cast<std::uint32_t>(std::stoul(arg.substr(13)));
        } else if (arg == "--keyfile" && i + 1 < args.size()) {
            keyfiles.emplace_back(args[++i]);
        } else if (arg == "--quick") {
            quick = true;
        }
    }

    if (!size.has_value()) {
        return output.error(ErrorCode::UsageError, "--size is required");
    }

    if (cred_options.inline_password.has_value()
        || cred_options.password_file.has_value()
        || cred_options.allow_env_password) {
        cred_options.confirm_password = false;
    }

    auto cred_result = read_credentials(ctx.platform, keyfiles, cred_options);
    if (!cred_result.is_ok()) {
        return output.error(cred_result.error(), cred_result.message());
    }
    auto cred = std::move(cred_result.value());

    vault::CreateVaultOptions options {};
    options.container_size = *size;
    options.quick_format = quick;
    if (chunk_size.has_value()) {
        options.chunk_size = *chunk_size;
    }
    apply_kdf_profile(kdf_profile, options);
    if (kdf_m_cost.has_value()) {
        options.kdf_params.m_cost_kib = *kdf_m_cost;
    }
    if (kdf_t_cost.has_value()) {
        options.kdf_params.t_cost = *kdf_t_cost;
    }
    if (kdf_p_cost.has_value()) {
        options.kdf_params.p_cost = *kdf_p_cost;
    }

    const auto created = vault_engine().create_new(path, cred, options);
    platform::secure_wipe(cred.password_utf8);
    if (!created.is_ok()) {
        return output.error(created.error(), created.message());
    }

    if (output.json_enabled()) {
        std::ostringstream json;
        json << "{\n";
        json << "  \"status\": \"created\",\n";
        json << "  \"path\": " << json_path(path) << ",\n";
        json << "  \"size\": " << *size << ",\n";
        json << "  \"format_version\": 1\n";
        json << '}';
        return output.emit_json(json.str());
    }

    return output.success("created vault: " + path.generic_string());
}

} 