#pragma once

#include <sv/platform/paths.hpp>
#include <sv/portable/portable_config.hpp>

namespace sv::portable {

platform::ResolvedPaths resolve_paths(
    const platform::PlatformContext& ctx,
    const PortableConfig& config);

} // namespace sv::portable