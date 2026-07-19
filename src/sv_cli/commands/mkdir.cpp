#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>

namespace sv::cli {

int mkdir_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("mkdir")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    const auto positional = collect_positional_args(args);
    if (positional.empty()) {
        return output.error(ErrorCode::UsageError, "usage: sv mkdir VAULT_DIR");
    }

    const auto container_path = resolve_vault_path(ctx, args, args.size());
    const auto found = ensure_vault_session(ctx, container_path, args);
    if (!found.is_ok()) {
        return output.error(found.error(), found.message());
    }

    const auto created = vault_engine().mkdir(*found.value(), positional[0]);
    if (!created.is_ok()) {
        return output.error(created.error(), created.message());
    }

    return output.success("created directory " + positional[0]);
}

} 