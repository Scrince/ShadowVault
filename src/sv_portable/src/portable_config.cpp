#include <sv/portable/portable_config.hpp>
#include <sv/portable/traveler_paths.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace sv::portable {

namespace {

std::string trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool parse_bool(const std::string& value, bool default_value) {
    const auto normalized = trim(value);
    if (normalized.empty()) {
        return default_value;
    }
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        return false;
    }
    return default_value;
}

std::string bool_to_ini(bool value) {
    return value ? "1" : "0";
}

using IniTable = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

Result<IniTable> parse_ini(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return Result<IniTable>::err(ErrorCode::IoError, "failed to open config file: " + path.string());
    }

    IniTable table;
    std::string current_section = "default";
    std::string line;

    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        if (line[0] == ';' || line[0] == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            current_section = trim(line.substr(1, line.size() - 2));
            continue;
        }

        const auto delimiter = line.find('=');
        if (delimiter == std::string::npos) {
            continue;
        }

        const auto key = trim(line.substr(0, delimiter));
        const auto value = trim(line.substr(delimiter + 1));
        table[current_section][key] = value;
    }

    return Result<IniTable>::ok(std::move(table));
}

const std::unordered_map<std::string, std::string>* section(
    const IniTable& table,
    const std::string& name) {
    const auto it = table.find(name);
    if (it == table.end()) {
        return nullptr;
    }
    return &it->second;
}

std::string lookup(
    const IniTable& table,
    const std::string& section_name,
    const std::string& key,
    const std::string& default_value = {}) {
    if (const auto* values = section(table, section_name)) {
        const auto it = values->find(key);
        if (it != values->end()) {
            return it->second;
        }
    }
    return default_value;
}

PortableConfig default_portable_config(const platform::PlatformContext& ctx) {
    PortableConfig config;
    config.portable_enabled = detect_portable_mode(ctx);
    config.no_host_traces = ctx.cli_flag_no_host_traces;
    config.recent_volumes_enabled = false;
    config.cache_enabled = false;
    config.log_to_file = false;
    config.vaults_dir = "vaults";
    config.keyfiles_dir = "keyfiles";
    config.temp_dir = ".shadowvault/temp";
    config.kdf_profile = config.portable_enabled ? "portable" : "desktop";
    return config;
}

PortableConfig config_from_ini(const IniTable& table, const PortableConfig& defaults) {
    PortableConfig config = defaults;
    config.portable_enabled = parse_bool(lookup(table, "portable", "enabled"), config.portable_enabled);
    config.no_host_traces = parse_bool(lookup(table, "portable", "no_host_traces"), config.no_host_traces);
    config.recent_volumes_enabled =
        parse_bool(lookup(table, "portable", "recent_volumes"), config.recent_volumes_enabled);
    config.cache_enabled = parse_bool(lookup(table, "portable", "cache_enabled"), config.cache_enabled);
    config.log_to_file = parse_bool(lookup(table, "portable", "log_to_file"), config.log_to_file);

    if (const auto vaults = lookup(table, "paths", "vaults_dir"); !vaults.empty()) {
        config.vaults_dir = vaults;
    }
    if (const auto keyfiles = lookup(table, "paths", "keyfiles_dir"); !keyfiles.empty()) {
        config.keyfiles_dir = keyfiles;
    }
    if (const auto temp = lookup(table, "paths", "temp_dir"); !temp.empty()) {
        config.temp_dir = temp;
    }
    if (const auto profile = lookup(table, "kdf", "profile"); !profile.empty()) {
        config.kdf_profile = profile;
    }
    return config;
}

} 

bool detect_portable_mode(const platform::PlatformContext& ctx) {
    if (ctx.cli_flag_portable) {
        return true;
    }
    if (platform::path_exists(ctx.exe_dir / "shadowvault.ini")) {
        return true;
    }
    if (ctx.exe_on_removable_volume) {
        return true;
    }
    return false;
}

std::filesystem::path default_config_path(const platform::PlatformContext& ctx) {
    if (ctx.config_override.has_value()) {
        return *ctx.config_override;
    }
    if (detect_portable_mode(ctx) || platform::path_exists(ctx.exe_dir / "shadowvault.ini")) {
        return ctx.exe_dir / "shadowvault.ini";
    }
    const auto host_config = platform::config_directory(false);
    if (host_config.is_ok()) {
        return host_config.value() / "config.ini";
    }
    return ctx.exe_dir / "config.ini";
}

Result<PortableConfig> load_config(
    const platform::PlatformContext& ctx,
    const std::optional<std::filesystem::path>& config_override) {
    auto config = default_portable_config(ctx);
    if (ctx.cli_flag_no_host_traces) {
        config.no_host_traces = true;
    }

    const auto path = config_override.value_or(default_config_path(ctx));
    if (!platform::path_exists(path)) {
        config.portable_enabled = detect_portable_mode(ctx);
        return Result<PortableConfig>::ok(std::move(config));
    }

    auto parsed = parse_ini(path);
    if (!parsed.is_ok()) {
        return Result<PortableConfig>::err(parsed.error(), parsed.message());
    }

    config = config_from_ini(parsed.value(), config);
    if (ctx.cli_flag_portable) {
        config.portable_enabled = true;
    } else if (!config.portable_enabled) {
        config.portable_enabled = detect_portable_mode(ctx);
    }
    if (ctx.cli_flag_no_host_traces) {
        config.no_host_traces = true;
    }
    if (config.no_host_traces) {
        config.recent_volumes_enabled = false;
        config.log_to_file = false;
    }

    return Result<PortableConfig>::ok(std::move(config));
}

Result<void> save_config(const std::filesystem::path& config_path, const PortableConfig& config) {
    std::error_code ec;
    const auto parent = config_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return Result<void>::err(ErrorCode::IoError, "failed to create config directory: " + ec.message());
        }
    }

    std::ofstream output(config_path);
    if (!output) {
        return Result<void>::err(ErrorCode::IoError, "failed to write config file: " + config_path.string());
    }

    output << "[portable]\n";
    output << "enabled=" << bool_to_ini(config.portable_enabled) << '\n';
    output << "no_host_traces=" << bool_to_ini(config.no_host_traces) << '\n';
    output << "cache_enabled=" << bool_to_ini(config.cache_enabled) << '\n';
    output << "log_to_file=" << bool_to_ini(config.log_to_file) << '\n';
    output << "recent_volumes=" << bool_to_ini(config.recent_volumes_enabled) << '\n';
    output << '\n';
    output << "[paths]\n";
    output << "vaults_dir=" << config.vaults_dir.generic_string() << '\n';
    output << "keyfiles_dir=" << config.keyfiles_dir.generic_string() << '\n';
    output << "temp_dir=" << config.temp_dir.generic_string() << '\n';
    output << '\n';
    output << "[kdf]\n";
    output << "profile=" << config.kdf_profile << '\n';

    return Result<void>::ok();
}

PortableStatus portable_status(const platform::PlatformContext& ctx) {
    const auto config_result = load_config(ctx, ctx.config_override);
    const auto config = config_result.is_ok() ? config_result.value() : default_portable_config(ctx);
    const auto paths = resolve_paths(ctx, config);

    PortableStatus status;
    status.portable_mode_active = config.portable_enabled;
    status.no_host_traces = config.no_host_traces;
    status.ini_found = platform::path_exists(ctx.exe_dir / "shadowvault.ini");
    status.removable_exe = ctx.exe_on_removable_volume;
    status.flag_forced = ctx.cli_flag_portable;
    status.exe_dir = ctx.exe_dir;
    status.config_path = paths.config_file;
    status.cache_dir = paths.cache_dir;
    status.temp_dir = paths.temp_dir;
    status.logs_dir = paths.logs_dir;
    if (!status.temp_dir.empty() && platform::is_directory(status.temp_dir)) {
        status.temp_files_pending = platform::count_regular_files(status.temp_dir);
    }

    if (status.portable_mode_active) {
        status.host_integration_blocked.push_back("mount");
        if (status.no_host_traces) {
            status.host_integration_blocked.push_back("favorites");
            status.host_integration_blocked.push_back("recent_volumes");
            status.host_integration_blocked.push_back("log_to_file");
        }
    }

    return status;
}

} 