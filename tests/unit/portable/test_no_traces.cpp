#include <sv/portable/portable_config.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>

namespace {

sv::platform::PlatformContext portable_ctx() {
    sv::platform::PlatformContext ctx {};
    ctx.exe_dir = std::filesystem::temp_directory_path();
    ctx.cli_flag_portable = true;
    ctx.cli_flag_no_host_traces = true;
    return ctx;
}

} 

TEST_CASE("P-03 no_host_traces disables recent volumes and log_to_file", "[unit][portable][p03]") {
    const auto config = sv::portable::load_config(portable_ctx(), std::nullopt);
    REQUIRE(config.is_ok());
    REQUIRE(config.value().no_host_traces);
    REQUIRE_FALSE(config.value().recent_volumes_enabled);
    REQUIRE_FALSE(config.value().log_to_file);
}

TEST_CASE("P-03 portable_status blocks host integration features", "[unit][portable][p03]") {
    const auto status = sv::portable::portable_status(portable_ctx());
    REQUIRE(status.portable_mode_active);
    REQUIRE(status.no_host_traces);
    REQUIRE(std::find(
                status.host_integration_blocked.begin(),
                status.host_integration_blocked.end(),
                "mount")
            != status.host_integration_blocked.end());
    REQUIRE(std::find(
                status.host_integration_blocked.begin(),
                status.host_integration_blocked.end(),
                "recent_volumes")
            != status.host_integration_blocked.end());
}