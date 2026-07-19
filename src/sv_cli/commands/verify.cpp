#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>

#include <sv/container/container_file.hpp>
#include <sv/vault/verify.hpp>

#include <sstream>

namespace sv::cli {

int verify_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("verify")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    const auto path = resolve_vault_path(ctx, args, 0);
    if (path.empty()) {
        return output.error(ErrorCode::UsageError, "usage: sv verify PATH");
    }

    bool deep = false;
    for (const auto& arg : args) {
        if (arg == "--deep") {
            deep = true;
        }
    }

    auto session = vault::SessionStore::instance().find(path);
    if (!session.is_ok()) {
        session = ensure_vault_session(ctx, path, args);
    }
    if (session.is_ok()) {
        const auto report = vault::verify_session(*session.value(), deep);
        if (!report.is_ok()) {
            return output.error(report.error(), report.message());
        }
        if (!report.value().crypto_ok) {
            return output.error(ErrorCode::CorruptContainer, report.value().message);
        }
        return output.success(report.value().message);
    }

    auto container = container::ContainerFileStore::open(path);
    if (!container.is_ok()) {
        return output.error(container.error(), container.message());
    }

    const auto report = vault::verify_structural(container.value());
    if (!report.is_ok()) {
        return output.error(report.error(), report.message());
    }

    if (output.json_enabled()) {
        std::ostringstream json;
        json << "{\n";
        json << "  \"structural_ok\": " << (report.value().structural_ok ? "true" : "false") << ",\n";
        json << "  \"message\": " << json_string(report.value().message) << "\n";
        json << '}';
        return output.emit_json(json.str());
    }

    if (!report.value().structural_ok) {
        return output.error(ErrorCode::CorruptContainer, report.value().message);
    }
    return output.success(report.value().message);
}

} 