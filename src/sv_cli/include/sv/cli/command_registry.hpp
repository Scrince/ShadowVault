#pragma once

#include <sv/cli/output.hpp>
#include <sv/platform/platform_context.hpp>
#include <sv/portable/portable_config.hpp>
#include <sv/portable/traveler_paths.hpp>

#include <functional>
#include <string>
#include <vector>

namespace sv::cli {

struct CliContext {
    GlobalOptions globals;
    platform::PlatformContext platform;
    portable::PortableConfig portable;
    platform::ResolvedPaths paths;
};

struct CommandDescriptor {
    std::string name;
    std::vector<std::string> aliases;
    std::string summary;
    std::function<int(CliContext&, const std::vector<std::string>&)> handler;
};

class CommandRegistry {
public:
    CommandRegistry();

    [[nodiscard]] const std::vector<CommandDescriptor>& commands() const { return commands_; }
    [[nodiscard]] const CommandDescriptor* find(const std::string& name) const;
    int dispatch(CliContext& ctx, const std::string& name, const std::vector<std::string>& args) const;
    void print_global_help(const Output& output) const;
    void print_command_help(const Output& output, const CommandDescriptor& command) const;

private:
    void register_command(CommandDescriptor descriptor);

    std::vector<CommandDescriptor> commands_;
};

bool args_contain_help(const std::vector<std::string>& args);
int run_not_implemented(CliContext& ctx, const std::string& command_name, const std::vector<std::string>& args);

} // namespace sv::cli