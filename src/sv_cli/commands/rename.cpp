#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>

namespace sv::cli {

int rename_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("rename")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    const auto positional = collect_positional_args(args);
    if (positional.size() < 2) {
        return output.error(ErrorCode::UsageError, "usage: sv rename FROM_PATH TO_PATH");
    }

    const auto container_path = resolve_vault_path(ctx, args, args.size());
    const auto found = ensure_vault_session(ctx, container_path, args);
    if (!found.is_ok()) {
        return output.error(found.error(), found.message());
    }

    const auto renamed = vault_engine().rename(*found.value(), positional[0], positional[1]);
    if (!renamed.is_ok()) {
        return output.error(renamed.error(), renamed.message());
    }

    return output.success("renamed " + positional[0] + " -> " + positional[1]);
}

} // namespace sv::cli