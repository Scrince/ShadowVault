#include "cli_args.hpp"

#include <sv/cli/command_registry.hpp>
#include <sv/cli/output.hpp>
#include <sv/platform/embedded_runtime.hpp>
#include <sv/portable/portable_config.hpp>
#include <sv/portable/traveler_paths.hpp>
#include <sv/version.hpp>

#include <iostream>
#include <sstream>

namespace {

int print_version(const sv::cli::GlobalOptions& globals) {
    sv::cli::Output output(globals);
    if (globals.json) {
        std::ostringstream json;
        json << "{\n";
        json << "  \"version\": " << sv::cli::json_string(SV_VERSION) << ",\n";
        json << "  \"format_version\": " << SV_FORMAT_VERSION << "\n";
        json << '}';
        return output.emit_json(json.str());
    }

    std::cout << "Shadow Vault CLI " << SV_VERSION << '\n';
    std::cout << "Format version: " << SV_FORMAT_VERSION << '\n';
    return 0;
}

sv::cli::CliContext build_context(const sv::cli::GlobalOptions& globals, const char* argv0) {
    sv::cli::CliContext ctx;
    ctx.globals = globals;
    ctx.platform = sv::platform::create_platform_context(
        argv0 != nullptr ? std::optional<std::filesystem::path>(argv0) : std::nullopt,
        globals.portable,
        globals.no_host_traces,
        globals.config_path);

    const auto config = sv::portable::load_config(ctx.platform, globals.config_path);
    if (config.is_ok()) {
        ctx.portable = config.value();
    } else {
        ctx.portable.portable_enabled = sv::portable::detect_portable_mode(ctx.platform);
        ctx.portable.no_host_traces = globals.no_host_traces;
    }
    ctx.paths = sv::portable::resolve_paths(ctx.platform, ctx.portable);
    return ctx;
}

} // namespace

int main(int argc, char** argv) {
    sv::platform::bootstrap_embedded_runtime();

    const auto parsed = sv::cli::parse_cli_arguments(argc, argv);
    if (!parsed.has_value()) {
        sv::cli::GlobalOptions globals;
        sv::cli::Output output(globals);
        return output.error(sv::ErrorCode::UsageError, "invalid command-line arguments");
    }

    if (parsed->globals.version) {
        return print_version(parsed->globals);
    }

    const sv::cli::CommandRegistry registry;
    const sv::cli::Output output(parsed->globals);

    if (parsed->globals.help && parsed->command.empty()) {
        registry.print_global_help(output);
        return 0;
    }

    if (parsed->command.empty()) {
        registry.print_global_help(output);
        return 0;
    }

    auto ctx = build_context(parsed->globals, argc > 0 ? argv[0] : nullptr);
    return registry.dispatch(ctx, parsed->command, parsed->command_args);
}