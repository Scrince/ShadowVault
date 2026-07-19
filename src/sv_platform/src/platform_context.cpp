#include <sv/platform/platform_context.hpp>
#include <sv/platform/paths.hpp>

#include <filesystem>
#include <system_error>

namespace sv::platform {

PlatformContext create_platform_context(
    const std::optional<std::filesystem::path>& argv0,
    bool cli_flag_portable,
    bool cli_flag_no_host_traces,
    const std::optional<std::filesystem::path>& config_override) {
    PlatformContext ctx;

    if (const auto exe_dir = executable_directory(); exe_dir.is_ok()) {
        ctx.exe_dir = exe_dir.value();
    } else if (argv0.has_value() && !argv0->empty()) {
        std::error_code ec;
        const auto absolute = std::filesystem::absolute(*argv0, ec);
        if (!ec) {
            ctx.exe_dir = absolute.parent_path();
        }
    }

    if (ctx.exe_dir.empty()) {
        std::error_code ec;
        const auto cwd = std::filesystem::current_path(ec);
        ctx.exe_dir = ec ? std::filesystem::path{} : cwd;
    }

    const auto removable = is_removable_drive(ctx.exe_dir);
    ctx.exe_on_removable_volume = removable.is_ok() && removable.value();
    ctx.cli_flag_portable = cli_flag_portable;
    ctx.cli_flag_no_host_traces = cli_flag_no_host_traces;
    ctx.config_override = config_override;
    return ctx;
}

} 