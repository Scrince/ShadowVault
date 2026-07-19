#include <sv/cli/command_registry.hpp>
#include <sv/cli/output.hpp>
#include <sv/portable/portable_config.hpp>

#include <sstream>

namespace sv::cli {

namespace {

std::string blocked_features_json(const portable::PortableStatus& status) {
    std::ostringstream json;
    json << '[';
    for (std::size_t i = 0; i < status.host_integration_blocked.size(); ++i) {
        json << json_string(status.host_integration_blocked[i]);
        if (i + 1 < status.host_integration_blocked.size()) {
            json << ", ";
        }
    }
    json << ']';
    return json.str();
}

} 

int portable_status_command(CliContext& ctx, const std::vector<std::string>& args) {
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("portable-status")) {
            Output output(ctx.globals);
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    const auto status = portable::portable_status(ctx.platform);
    Output output(ctx.globals);

    if (output.json_enabled()) {
        std::ostringstream json;
        json << "{\n";
        json << "  \"status\": \"ok\",\n";
        json << "  \"portable_mode_active\": " << (status.portable_mode_active ? "true" : "false") << ",\n";
        json << "  \"no_host_traces\": " << (status.no_host_traces ? "true" : "false") << ",\n";
        json << "  \"ini_found\": " << (status.ini_found ? "true" : "false") << ",\n";
        json << "  \"removable_exe\": " << (status.removable_exe ? "true" : "false") << ",\n";
        json << "  \"flag_forced\": " << (status.flag_forced ? "true" : "false") << ",\n";
        json << "  \"exe_dir\": " << json_path(status.exe_dir) << ",\n";
        json << "  \"config_path\": " << json_path(status.config_path) << ",\n";
        json << "  \"cache_dir\": " << json_path(status.cache_dir) << ",\n";
        json << "  \"temp_dir\": " << json_path(status.temp_dir) << ",\n";
        json << "  \"logs_dir\": " << json_path(status.logs_dir) << ",\n";
        json << "  \"host_integration_blocked\": " << blocked_features_json(status) << ",\n";
        json << "  \"temp_files_pending\": " << status.temp_files_pending << "\n";
        json << '}';
        return output.emit_json(json.str());
    }

    output.info(std::string("portable_mode: ") + (status.portable_mode_active ? "active" : "inactive"));
    output.info(std::string("no_host_traces: ") + (status.no_host_traces ? "enabled" : "disabled"));
    output.info("exe_dir: " + status.exe_dir.generic_string());
    output.info("config_path: " + status.config_path.generic_string());
    output.info("cache_dir: " + status.cache_dir.generic_string());
    output.info("temp_dir: " + status.temp_dir.generic_string());
    output.info("logs_dir: " + status.logs_dir.generic_string());
    if (!status.host_integration_blocked.empty()) {
        std::ostringstream blocked;
        blocked << "host_integration_blocked:";
        for (const auto& feature : status.host_integration_blocked) {
            blocked << ' ' << feature;
        }
        output.info(blocked.str());
    }
    output.info("temp_files_pending: " + std::to_string(status.temp_files_pending));
    return 0;
}

} 