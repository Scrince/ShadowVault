#pragma once

#include <sv/platform/platform_context.hpp>
#include <sv/result.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sv::portable {

struct PortableConfig {
    bool portable_enabled{false};
    bool no_host_traces{false};
    bool recent_volumes_enabled{false};
    bool cache_enabled{false};
    bool log_to_file{false};
    std::filesystem::path vaults_dir;
    std::filesystem::path keyfiles_dir;
    std::filesystem::path temp_dir;
    std::string kdf_profile{"desktop"};
};

struct PortableStatus {
    bool portable_mode_active{false};
    bool no_host_traces{false};
    bool ini_found{false};
    bool removable_exe{false};
    bool flag_forced{false};
    std::filesystem::path exe_dir;
    std::filesystem::path config_path;
    std::filesystem::path cache_dir;
    std::filesystem::path temp_dir;
    std::filesystem::path logs_dir;
    std::vector<std::string> host_integration_blocked;
    std::uintmax_t temp_files_pending{0};
};

bool detect_portable_mode(const platform::PlatformContext& ctx);

Result<PortableConfig> load_config(
    const platform::PlatformContext& ctx,
    const std::optional<std::filesystem::path>& config_override = std::nullopt);

Result<void> save_config(
    const std::filesystem::path& config_path,
    const PortableConfig& config);

PortableStatus portable_status(const platform::PlatformContext& ctx);

std::filesystem::path default_config_path(const platform::PlatformContext& ctx);

} 