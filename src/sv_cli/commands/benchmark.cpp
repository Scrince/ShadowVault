#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>

#include <sv/crypto/kdf_engine.hpp>
#include <sv/vault/vault_engine.hpp>

#include <chrono>
#include <sstream>

namespace sv::cli {

int benchmark_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("benchmark")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    std::string profile = "desktop";
    for (const auto& arg : args) {
        if (arg.rfind("--kdf-profile=", 0) == 0) {
            profile = arg.substr(14);
        } else if (arg == "--kdf-profile" && !args.empty()) {
            continue;
        }
    }

    if (!crypto::KdfEngine::is_available()) {
        return output.error(ErrorCode::NotImplemented, "KDF engine unavailable");
    }

    vault::CreateVaultOptions options {};
    apply_kdf_profile(profile, options);
    const auto& params = options.kdf_params;
    std::array<std::uint8_t, 32> password {};
    std::array<std::uint8_t, 32> salt {};
    std::array<std::uint8_t, 64> derived {};

    const auto start = std::chrono::steady_clock::now();
    const auto rc = crypto::KdfEngine::argon2id(password, salt, params, derived);
    const auto end = std::chrono::steady_clock::now();
    if (!rc.is_ok()) {
        return output.error(rc.error(), rc.message());
    }

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (output.json_enabled()) {
        std::ostringstream json;
        json << "{\n";
        json << "  \"kdf_ms\": " << ms << ",\n";
        json << "  \"profile\": " << json_string(profile) << "\n";
        json << '}';
        return output.emit_json(json.str());
    }

    return output.success("KDF benchmark (" + profile + "): " + std::to_string(ms) + " ms");
}

} 