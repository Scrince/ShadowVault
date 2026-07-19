#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>

namespace sv::cli {

int remove_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("remove")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    const auto positional = collect_positional_args(args);
    if (positional.empty()) {
        return output.error(ErrorCode::UsageError, "usage: sv remove VAULT_PATH");
    }

    const auto container_path = resolve_vault_path(ctx, args, args.size());
    const auto found = ensure_vault_session(ctx, container_path, args);
    if (!found.is_ok()) {
        return output.error(found.error(), found.message());
    }

    const auto removed = vault_engine().remove(*found.value(), positional[0]);
    if (!removed.is_ok()) {
        return output.error(removed.error(), removed.message());
    }

    return output.success("removed " + positional[0]);
}

} 