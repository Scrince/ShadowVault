#include <sv/platform/file_io.hpp>
#include <sv/portable/portable_config.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <filesystem>

namespace {

sv::platform::PlatformContext make_ctx(const std::filesystem::path& exe_dir) {
    sv::platform::PlatformContext ctx {};
    ctx.exe_dir = exe_dir;
    ctx.cli_flag_portable = false;
    ctx.cli_flag_no_host_traces = false;
    ctx.exe_on_removable_volume = false;
    return ctx;
}

} 

TEST_CASE("P-02 detect_portable_mode from shadowvault.ini", "[unit][portable][p02]") {
    const auto base = std::filesystem::temp_directory_path() / "sv_portable_detect";
    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    std::filesystem::create_directories(base, ec);

    const auto ini_path = base / "shadowvault.ini";
    REQUIRE(sv::platform::write_file(
                ini_path,
                sv::as_bytes(sv::Span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>("[portable]\nenabled=1\n"),
                    18)))
                .is_ok());

    const auto ctx = make_ctx(base);
    REQUIRE(sv::portable::detect_portable_mode(ctx));

    std::filesystem::remove_all(base, ec);
}

TEST_CASE("P-02 detect_portable_mode from CLI flag", "[unit][portable][p02]") {
    auto ctx = make_ctx(std::filesystem::temp_directory_path());
    ctx.cli_flag_portable = true;
    REQUIRE(sv::portable::detect_portable_mode(ctx));
}

TEST_CASE("P-02 load_config reads INI values", "[unit][portable][p02]") {
    const auto base = std::filesystem::temp_directory_path() / "sv_portable_ini";
    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    std::filesystem::create_directories(base, ec);

    const auto ini_path = base / "shadowvault.ini";
    const char* ini =
        "[portable]\n"
        "enabled=1\n"
        "no_host_traces=1\n"
        "[paths]\n"
        "vaults_dir=custom_vaults\n"
        "[kdf]\n"
        "profile=portable\n";
    REQUIRE(sv::platform::write_file(
                ini_path,
                sv::as_bytes(sv::Span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(ini),
                    std::strlen(ini))))
                .is_ok());

    auto ctx = make_ctx(base);
    const auto config = sv::portable::load_config(ctx, ini_path);
    REQUIRE(config.is_ok());
    REQUIRE(config.value().portable_enabled);
    REQUIRE(config.value().no_host_traces);
    REQUIRE(config.value().vaults_dir.generic_string() == "custom_vaults");
    REQUIRE(config.value().kdf_profile == "portable");

    std::filesystem::remove_all(base, ec);
}