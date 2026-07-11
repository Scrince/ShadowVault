#include <sv/platform/paths.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace sv::platform;

TEST_CASE("normalize_path collapses dot segments", "[platform][paths]") {
    const auto result = normalize_path(std::filesystem::path("foo/bar/../baz"));
    REQUIRE(result.is_ok());

#if defined(_WIN32)
    REQUIRE(result.value() == std::filesystem::path("foo\\baz"));
#else
    REQUIRE(result.value() == std::filesystem::path("foo/baz"));
#endif
}

TEST_CASE("normalize_path rejects empty path", "[platform][paths]") {
    const auto result = normalize_path(std::filesystem::path{});
    REQUIRE(!result.is_ok());
    REQUIRE(result.error() == sv::ErrorCode::InvalidArgument);
}

TEST_CASE("executable_directory returns absolute path", "[platform][paths]") {
    const auto result = executable_directory();
    REQUIRE(result.is_ok());
    REQUIRE(result.value().is_absolute());
}

TEST_CASE("config_directory portable uses executable directory", "[platform][paths]") {
    const auto exe_dir = executable_directory();
    const auto portable_dir = config_directory(true);

    REQUIRE(exe_dir.is_ok());
    REQUIRE(portable_dir.is_ok());
    REQUIRE(portable_dir.value() == exe_dir.value());
}

TEST_CASE("config_directory non-portable returns absolute path", "[platform][paths]") {
    const auto result = config_directory(false);
    REQUIRE(result.is_ok());
    REQUIRE(result.value().is_absolute());
}

TEST_CASE("temp_directory returns absolute path", "[platform][paths]") {
    const auto result = temp_directory();
    REQUIRE(result.is_ok());
    REQUIRE(result.value().is_absolute());
}

TEST_CASE("is_removable_drive handles system path", "[platform][paths]") {
    const auto exe_dir = executable_directory();
    REQUIRE(exe_dir.is_ok());

    const auto result = is_removable_drive(exe_dir.value());
    REQUIRE(result.is_ok());
    // Smoke check only; value depends on host media.
    REQUIRE((result.value() == true || result.value() == false));
}