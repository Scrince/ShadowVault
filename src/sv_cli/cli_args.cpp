#include "cli_args.hpp"

#include <filesystem>

namespace sv::cli {

namespace {

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool is_known_global_flag(const std::string& arg) {
    return arg == "--help" || arg == "-h" || arg == "--version" || arg == "--json" || arg == "--quiet"
        || arg == "-q" || arg == "--verbose" || arg == "-v" || arg == "--portable" || arg == "--no-host-traces"
        || arg == "--replace-session" || arg == "--config" || arg == "--vault" || arg == "-V"
        || starts_with(arg, "--config=") || starts_with(arg, "--vault=") || starts_with(arg, "-V=");
}

bool take_path_value(
    const std::string& arg,
    const std::string& option_name,
    std::optional<std::filesystem::path>& out_path,
    std::string& error) {
    const auto eq_prefix = option_name + '=';
    if (arg == option_name) {
        error = option_name + " requires a path argument";
        return false;
    }
    if (starts_with(arg, eq_prefix)) {
        const auto value = arg.substr(eq_prefix.size());
        if (value.empty()) {
            error = option_name + " requires a non-empty path";
            return false;
        }
        out_path = std::filesystem::path(value);
        return true;
    }
    return false;
}

bool parse_global_option(
    const std::string& arg,
    GlobalOptions& globals,
    std::string& error) {
    if (arg == "--help" || arg == "-h") {
        globals.help = true;
        return true;
    }
    if (arg == "--version") {
        globals.version = true;
        return true;
    }
    if (arg == "--json") {
        globals.json = true;
        return true;
    }
    if (arg == "--quiet" || arg == "-q") {
        globals.quiet = true;
        return true;
    }
    if (arg == "--verbose" || arg == "-v") {
        globals.verbose = true;
        return true;
    }
    if (arg == "--portable") {
        globals.portable = true;
        return true;
    }
    if (arg == "--no-host-traces") {
        globals.no_host_traces = true;
        return true;
    }
    if (arg == "--replace-session") {
        return true;
    }

    if (take_path_value(arg, "--config", globals.config_path, error)) {
        return true;
    }
    if (take_path_value(arg, "--vault", globals.vault_path, error)) {
        return true;
    }
    if (take_path_value(arg, "-V", globals.vault_path, error)) {
        return true;
    }

    if (arg == "--config" || arg == "--vault" || arg == "-V") {
        error = arg + " requires a path argument";
        return false;
    }

    if (is_known_global_flag(arg)) {
        return true;
    }

    if (!arg.empty() && arg[0] == '-') {
        error = "unknown option: " + arg;
        return false;
    }

    return false;
}

} // namespace

std::optional<ParsedInvocation> parse_cli_arguments(int argc, char** argv) {
    ParsedInvocation parsed;
    bool parsing_globals = true;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (parsing_globals && arg == "--") {
            parsing_globals = false;
            continue;
        }

        if (parsing_globals && !arg.empty() && arg[0] != '-') {
            parsing_globals = false;
            parsed.command = arg;
            continue;
        }

        if (parsing_globals) {
            if (arg == "--config" || arg == "--vault" || arg == "-V") {
                if (i + 1 >= argc) {
                    return std::nullopt;
                }
                const auto value = std::string(argv[++i]);
                if (value.empty()) {
                    return std::nullopt;
                }
                if (arg == "--config") {
                    parsed.globals.config_path = std::filesystem::path(value);
                } else {
                    parsed.globals.vault_path = std::filesystem::path(value);
                }
                continue;
            }

            std::string error;
            if (parse_global_option(arg, parsed.globals, error)) {
                continue;
            }
            if (!error.empty()) {
                return std::nullopt;
            }
            return std::nullopt;
        }

        parsed.command_args.push_back(arg);
    }

    return parsed;
}

} // namespace sv::cli