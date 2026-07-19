#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>

#include <sstream>

namespace sv::cli {

int list_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("list")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    const auto positional = collect_positional_args(args);
    std::filesystem::path vault_dir = "/";
    if (!positional.empty()) {
        vault_dir = positional[0];
    }

    const auto container_path = resolve_vault_path(ctx, args, args.size());
    const auto found = ensure_vault_session(ctx, container_path, args);
    if (!found.is_ok()) {
        return output.error(found.error(), found.message());
    }

    const auto entries = vault_engine().list(*found.value(), vault_dir);
    if (!entries.is_ok()) {
        return output.error(entries.error(), entries.message());
    }

    if (output.json_enabled()) {
        std::ostringstream json;
        json << "{\n  \"entries\": [\n";
        for (std::size_t i = 0; i < entries.value().size(); ++i) {
            const auto& entry = entries.value()[i];
            json << "    {\"name\": " << json_string(entry.name)
                 << ", \"type\": " << json_string(entry.type)
                 << ", \"size\": " << entry.size
                 << ", \"mtime\": " << entry.mtime << '}';
            if (i + 1 < entries.value().size()) {
                json << ',';
            }
            json << '\n';
        }
        json << "  ]\n}";
        return output.emit_json(json.str());
    }

    for (const auto& entry : entries.value()) {
        output.info(entry.type + "\t" + entry.name);
    }
    return 0;
}

} 