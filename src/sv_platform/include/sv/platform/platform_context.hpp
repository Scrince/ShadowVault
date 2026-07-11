#pragma once

#include <filesystem>
#include <optional>

namespace sv::platform {

struct PlatformContext {
    std::filesystem::path exe_dir;
    bool exe_on_removable_volume{false};
    bool cli_flag_portable{false};
    bool cli_flag_no_host_traces{false};
    std::optional<std::filesystem::path> config_override;
};

PlatformContext create_platform_context(
    const std::optional<std::filesystem::path>& argv0 = std::nullopt,
    bool cli_flag_portable = false,
    bool cli_flag_no_host_traces = false,
    const std::optional<std::filesystem::path>& config_override = std::nullopt);

} // namespace sv::platform