#pragma once

#include <sv/cli/output.hpp>

#include <optional>
#include <string>
#include <vector>

namespace sv::cli {

struct ParsedInvocation {
    GlobalOptions globals;
    std::string command;
    std::vector<std::string> command_args;
};


[[nodiscard]] std::optional<ParsedInvocation> parse_cli_arguments(int argc, char** argv);

} 