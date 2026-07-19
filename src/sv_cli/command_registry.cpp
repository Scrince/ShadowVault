#include <sv/cli/command_registry.hpp>

#include <sv/version.hpp>

#include <algorithm>
#include <iostream>
#include <sstream>

namespace sv::cli {

int create_command(CliContext& ctx, const std::vector<std::string>& args);
int unlock_command(CliContext& ctx, const std::vector<std::string>& args);
int lock_command(CliContext& ctx, const std::vector<std::string>& args);
int put_command(CliContext& ctx, const std::vector<std::string>& args);
int get_command(CliContext& ctx, const std::vector<std::string>& args);
int list_command(CliContext& ctx, const std::vector<std::string>& args);
int remove_command(CliContext& ctx, const std::vector<std::string>& args);
int rename_command(CliContext& ctx, const std::vector<std::string>& args);
int mkdir_command(CliContext& ctx, const std::vector<std::string>& args);
int verify_command(CliContext& ctx, const std::vector<std::string>& args);
int repair_command(CliContext& ctx, const std::vector<std::string>& args);
int change_password_command(CliContext& ctx, const std::vector<std::string>& args);
int add_keyfile_command(CliContext& ctx, const std::vector<std::string>& args);
int remove_keyfile_command(CliContext& ctx, const std::vector<std::string>& args);
int backup_header_command(CliContext& ctx, const std::vector<std::string>& args);
int restore_header_command(CliContext& ctx, const std::vector<std::string>& args);
int create_hidden_command(CliContext& ctx, const std::vector<std::string>& args);
int mount_command(CliContext& ctx, const std::vector<std::string>& args);
int dismount_command(CliContext& ctx, const std::vector<std::string>& args);
int benchmark_command(CliContext& ctx, const std::vector<std::string>& args);
int portable_status_command(CliContext& ctx, const std::vector<std::string>& args);

bool args_contain_help(const std::vector<std::string>& args) {
    return std::any_of(args.begin(), args.end(), [](const std::string& arg) {
        return arg == "--help" || arg == "-h";
    });
}

int run_not_implemented(CliContext& ctx, const std::string& command_name, const std::vector<std::string>& args) {
    CommandRegistry registry;
    if (args_contain_help(args)) {
        if (const auto* command = registry.find(command_name)) {
            Output output(ctx.globals);
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    Output output(ctx.globals);
    return output.error(ErrorCode::NotImplemented, command_name + " is not yet implemented");
}

CommandRegistry::CommandRegistry() {
    register_command({"create", {}, "Create a new encrypted vault", create_command});
    register_command({"unlock", {"open"}, "Unlock a vault", unlock_command});
    register_command({"lock", {"close"}, "Lock vault and wipe keys", lock_command});
    register_command({"put", {}, "Add/update file in vault", put_command});
    register_command({"get", {}, "Extract file from vault", get_command});
    register_command({"list", {}, "List directory contents", list_command});
    register_command({"remove", {}, "Remove file or directory", remove_command});
    register_command({"rename", {}, "Rename or move within vault", rename_command});
    register_command({"mkdir", {}, "Create directory", mkdir_command});
    register_command({"verify", {}, "Verify vault integrity", verify_command});
    register_command({"repair", {}, "Repair corrupted vault", repair_command});
    register_command({"change-password", {}, "Change password/keyfiles", change_password_command});
    register_command({"add-keyfile", {}, "Add keyfile to vault", add_keyfile_command});
    register_command({"remove-keyfile", {}, "Remove keyfile from vault", remove_keyfile_command});
    register_command({"backup-header", {}, "Export volume header", backup_header_command});
    register_command({"restore-header", {}, "Restore volume header", restore_header_command});
    register_command({"create-hidden", {}, "Create hidden volume", create_hidden_command});
    register_command({"mount", {}, "Mount vault as drive (experimental)", mount_command});
    register_command({"dismount", {}, "Dismount vault", dismount_command});
    register_command({"benchmark", {}, "Run crypto benchmark", benchmark_command});
    register_command({"portable-status", {}, "Show portable mode status", portable_status_command});
}

void CommandRegistry::register_command(CommandDescriptor descriptor) {
    commands_.push_back(std::move(descriptor));
}

const CommandDescriptor* CommandRegistry::find(const std::string& name) const {
    for (const auto& command : commands_) {
        if (command.name == name) {
            return &command;
        }
        if (std::find(command.aliases.begin(), command.aliases.end(), name) != command.aliases.end()) {
            return &command;
        }
    }
    return nullptr;
}

int CommandRegistry::dispatch(CliContext& ctx, const std::string& name, const std::vector<std::string>& args) const {
    const auto* command = find(name);
    if (command == nullptr) {
        Output output(ctx.globals);
        return output.error(ErrorCode::UsageError, "unknown command: " + name);
    }
    return command->handler(ctx, args);
}

void CommandRegistry::print_global_help(const Output& output) const {
    if (output.json_enabled()) {
        std::ostringstream json;
        json << "{\n";
        json << "  \"name\": \"Shadow Vault CLI\",\n";
        json << "  \"version\": " << json_string(SV_VERSION) << ",\n";
        json << "  \"format_version\": " << SV_FORMAT_VERSION << ",\n";
        json << "  \"commands\": [\n";
        for (std::size_t i = 0; i < commands_.size(); ++i) {
            const auto& command = commands_[i];
            json << "    {\"name\": " << json_string(command.name)
                 << ", \"summary\": " << json_string(command.summary) << '}';
            if (i + 1 < commands_.size()) {
                json << ',';
            }
            json << '\n';
        }
        json << "  ]\n";
        json << '}';
        output.emit_json(json.str());
        return;
    }

    std::cout << "Shadow Vault CLI " << SV_VERSION << '\n';
    std::cout << "Usage: sv <command> [options]\n\n";
    std::cout << "Commands:\n";
    for (const auto& command : commands_) {
        std::cout << "  " << command.name;
        if (!command.aliases.empty()) {
            std::cout << '|' << command.aliases.front();
        }
        std::cout << "  " << command.summary << '\n';
    }
    std::cout << "\nRun 'sv <command> --help' for command help.\n";
}

void CommandRegistry::print_command_help(const Output& output, const CommandDescriptor& command) const {
    if (output.json_enabled()) {
        std::ostringstream json;
        json << "{\n";
        json << "  \"name\": " << json_string(command.name) << ",\n";
        json << "  \"summary\": " << json_string(command.summary) << ",\n";
        json << "  \"status\": \"implemented\"\n";
        json << '}';
        output.emit_json(json.str());
        return;
    }

    std::cout << "Shadow Vault CLI " << SV_VERSION << '\n';
    std::cout << "Usage: sv " << command.name << " [options]\n\n";
    std::cout << command.summary << '\n';
    std::cout << "\nRun 'sv " << command.name << " --help' with --json for machine-readable help.\n";
}

} 