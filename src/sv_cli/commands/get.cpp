#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>

#include <sv/platform/file_io.hpp>

namespace sv::cli {

int get_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("get")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    const auto positional = collect_positional_args(args);
    if (positional.size() < 2) {
        return output.error(ErrorCode::UsageError, "usage: sv get VAULT_PATH LOCAL_PATH");
    }

    const std::filesystem::path vault_path = positional[0];
    const std::filesystem::path local_path = positional[1];

    const auto container_path = resolve_vault_path(ctx, args, args.size());
    const auto found = ensure_vault_session(ctx, container_path, args);
    if (!found.is_ok()) {
        return output.error(found.error(), found.message());
    }

    const auto data = vault_engine().get(*found.value(), vault_path);
    if (!data.is_ok()) {
        return output.error(data.error(), data.message());
    }

    const auto write = platform::write_file(local_path, as_bytes(data.value()), true);
    if (!write.is_ok()) {
        return output.error(write.error(), write.message());
    }

    return output.success("extracted " + vault_path.generic_string());
}

} 