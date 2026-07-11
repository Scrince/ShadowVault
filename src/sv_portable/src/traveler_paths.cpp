#include <sv/portable/traveler_paths.hpp>

namespace sv::portable {

namespace {

std::filesystem::path make_relative_path(
    const std::filesystem::path& exe_dir,
    const std::filesystem::path& configured) {
    if (configured.is_absolute()) {
        return configured;
    }
    return exe_dir / configured;
}

} // namespace

platform::ResolvedPaths resolve_paths(
    const platform::PlatformContext& ctx,
    const PortableConfig& config) {
    if (!config.portable_enabled) {
        auto paths = platform::default_paths();
        if (ctx.config_override.has_value()) {
            paths.config_file = *ctx.config_override;
        }
        return paths;
    }

    const auto data_root = ctx.exe_dir / ".shadowvault";
    platform::ResolvedPaths paths;
    paths.config_dir = ctx.exe_dir;
    paths.config_file = ctx.config_override.value_or(ctx.exe_dir / "shadowvault.ini");
    paths.temp_dir = make_relative_path(ctx.exe_dir, config.temp_dir);
    paths.cache_dir = config.cache_enabled ? (data_root / "cache") : paths.temp_dir;
    paths.logs_dir = config.log_to_file ? (data_root / "logs") : paths.temp_dir;
    paths.recent_volumes_file =
        config.recent_volumes_enabled ? (data_root / "recent.json") : std::filesystem::path{};
    return paths;
}

} // namespace sv::portable