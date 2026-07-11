#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>

namespace sv::cli {

int lock_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("lock")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    std::filesystem::path path = resolve_vault_path(ctx, args, 0);
    if (path.empty()) {
        const auto any = vault::SessionStore::instance().find_any();
        if (!any.is_ok()) {
            return output.error(any.error(), any.message());
        }
        path = any.value()->path;
    }

    const auto locked = vault_engine().lock(path);
    if (!locked.is_ok()) {
        return output.error(locked.error(), locked.message());
    }

    return output.success("locked vault: " + path.generic_string());
}

} // namespace sv::cli