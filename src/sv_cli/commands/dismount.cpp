#include <sv/cli/command_registry.hpp>
#include <sv/cli/output.hpp>
#include <sv/exit_codes.hpp>

#if defined(SV_HAVE_MOUNT)
#include <sv/mount/mount_manager.hpp>
#endif

#include <filesystem>
#include <string>

namespace sv::cli {

namespace {

bool parse_flag(const std::vector<std::string>& args, const std::string& flag) {
    for (const auto& arg : args) {
        if (arg == flag) {
            return true;
        }
    }
    return false;
}

std::string parse_option(const std::vector<std::string>& args, const std::string& option) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == option && i + 1 < args.size()) {
            return args[i + 1];
        }
        const auto prefix = option + "=";
        if (args[i].rfind(prefix, 0) == 0) {
            return args[i].substr(prefix.size());
        }
    }
    return {};
}

} 

int dismount_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output out(ctx.globals);

#if !defined(SV_HAVE_MOUNT)
    return out.error(ErrorCode::NotImplemented, "dismount requires SV_BUILD_MOUNT=ON");
#else
    if (args_contain_help(args)) {
        out.info("Usage: sv dismount [--point MOUNT] [--all]");
        return exit_codes::success;
    }

    auto& manager = mount::MountManager::instance();

    if (parse_flag(args, "--all")) {
        auto result = manager.dismount_all();
        if (!result.is_ok()) {
            return out.error(
                result.error(),
                result.message().empty() ? error_code_name(result.error()) : result.message());
        }
        return out.success("All mount sessions dismounted");
    }

    const auto mount_point = parse_option(args, "--point");
    if (!mount_point.empty()) {
        auto result = manager.dismount_point(std::filesystem::path(mount_point));
        if (!result.is_ok()) {
            return out.error(
                result.error(),
                result.message().empty() ? error_code_name(result.error()) : result.message());
        }
        return out.success("Dismounted " + mount_point);
    }

    return out.error(ErrorCode::UsageError, "dismount requires --point or --all");
#endif
}

} 