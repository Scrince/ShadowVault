#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>

#include <sv/platform/file_io.hpp>

namespace sv::cli {

int put_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("put")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    const auto positional = collect_positional_args(args);
    if (positional.size() < 2) {
        return output.error(ErrorCode::UsageError, "usage: sv put LOCAL_PATH VAULT_PATH");
    }

    const std::filesystem::path local_path = positional[0];
    const std::filesystem::path vault_path = positional[1];
    bool force = false;
    for (const auto& arg : args) {
        if (arg == "--force") {
            force = true;
        }
    }

    const auto container_path = resolve_vault_path(ctx, args, args.size());
    const auto found = ensure_vault_session(ctx, container_path, args);
    if (!found.is_ok()) {
        return output.error(found.error(), found.message());
    }
    vault::UnlockedVault* session = found.value();

    const auto data = platform::read_file(local_path, 256ULL * 1024 * 1024);
    if (!data.is_ok()) {
        return output.error(data.error(), data.message());
    }

    const auto put = vault_engine().put(*session, vault_path, data.value(), force);
    if (!put.is_ok()) {
        return output.error(put.error(), put.message());
    }

    return output.success("stored " + vault_path.generic_string());
}

} // namespace sv::cli