#include "cli_args.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

std::vector<char*> argv_from_strings(std::vector<std::string>& storage) {
    std::vector<char*> argv;
    argv.reserve(storage.size());
    for (auto& arg : storage) {
        argv.push_back(arg.data());
    }
    return argv;
}

} 

TEST_CASE("parse_cli_arguments accepts --config=value", "[cli][args]") {
    std::vector<std::string> args_storage = {"svcli", "--config=test.ini", "portable-status"};
    auto argv = argv_from_strings(args_storage);

    const auto parsed = sv::cli::parse_cli_arguments(static_cast<int>(argv.size()), argv.data());
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->command == "portable-status");
    REQUIRE(parsed->globals.config_path.has_value());
    REQUIRE(parsed->globals.config_path->generic_string() == "test.ini");
}

TEST_CASE("parse_cli_arguments accepts --config path", "[cli][args]") {
    std::vector<std::string> args_storage = {"svcli", "--config", "test.ini", "portable-status"};
    auto argv = argv_from_strings(args_storage);

    const auto parsed = sv::cli::parse_cli_arguments(static_cast<int>(argv.size()), argv.data());
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->globals.config_path->generic_string() == "test.ini");
}

TEST_CASE("parse_cli_arguments rejects unknown global option", "[cli][args]") {
    std::vector<std::string> args_storage = {"svcli", "--bogus", "portable-status"};
    auto argv = argv_from_strings(args_storage);

    const auto parsed = sv::cli::parse_cli_arguments(static_cast<int>(argv.size()), argv.data());
    REQUIRE_FALSE(parsed.has_value());
}