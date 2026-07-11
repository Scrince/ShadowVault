#include <sv/cli/command_registry.hpp>
#include <sv/cli/output.hpp>
#include <sv/cli/vault_cmd.hpp>
#include <sv/exit_codes.hpp>

#if defined(SV_HAVE_MOUNT)
#include <sv/mount/mount_manager.hpp>
#include <sv/platform/platform_context.hpp>
#include <sv/platform/terminal_secret.hpp>
#include <sv/portable/portable_config.hpp>
#include <sv/vault/session_store.hpp>
#endif

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace sv::cli {

namespace {

bool parse_flag(const std::vector<std::string>& args, const std::string& flag) {
    for (const auto& arg : args) {
        if (arg == flag) {
            return true;
        }
    }
    return false;
}

std::string parse_option(const std::vector<std::string>& args, const std::string& option) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == option && i + 1 < args.size()) {
            return args[i + 1];
        }
        const auto prefix = option + "=";
        if (args[i].rfind(prefix, 0) == 0) {
            return args[i].substr(prefix.size());
        }
    }
    return {};
}

#if defined(SV_HAVE_MOUNT)

std::atomic<bool> g_mount_interrupt_requested {false};

#if defined(_WIN32)
BOOL WINAPI mount_console_handler(const DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT || ctrl_type == CTRL_CLOSE_EVENT) {
        g_mount_interrupt_requested.store(true, std::memory_order_release);
        return TRUE;
    }
    return FALSE;
}
#endif

void wait_for_mount_shutdown(mount::MountManager& manager, const std::filesystem::path& mount_point) {
#if defined(_WIN32)
    SetConsoleCtrlHandler(mount_console_handler, TRUE);
#endif

    while (!g_mount_interrupt_requested.load(std::memory_order_acquire)) {
        manager.on_idle_tick();

        bool still_active = false;
        for (const auto& session : manager.list_sessions()) {
            if (session.mount_point == mount_point) {
                still_active = true;
                break;
            }
        }
        if (!still_active) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

#if defined(_WIN32)
    SetConsoleCtrlHandler(nullptr, FALSE);
#endif

    (void)manager.dismount_point(mount_point);
}

#endif

} // namespace

int mount_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output out(ctx.globals);

#if !defined(SV_HAVE_MOUNT)
    return out.error(ErrorCode::NotImplemented, "mount requires SV_BUILD_MOUNT=ON");
#else
    if (args_contain_help(args)) {
        out.info(
            "Usage: sv mount PATH --point MOUNT [--password PASS] [--read-only] "
            "[--protect-hidden] [--allow-host-integration] [--idle-timeout SEC]");
        return exit_codes::success;
    }

    if (args.empty()) {
        return out.error(ErrorCode::UsageError, "mount requires vault PATH");
    }

    const auto mount_point = parse_option(args, "--point");
    if (mount_point.empty()) {
        return out.error(ErrorCode::UsageError, "mount requires --point");
    }

    const auto status = portable::portable_status(ctx.platform);
    if (status.no_host_traces && !parse_flag(args, "--allow-host-integration")) {
        return out.error(
            ErrorCode::InvalidArgument,
            "mount blocked in no-host-traces mode (use --allow-host-integration)");
    }

    mount::MountOptions opts;
    opts.mount_point = mount_point;
    opts.read_only = parse_flag(args, "--read-only");
    opts.protect_hidden = parse_flag(args, "--protect-hidden");

    const auto idle_seconds = parse_option(args, "--idle-timeout");
    if (!idle_seconds.empty()) {
        opts.idle_timeout = std::chrono::seconds(std::stoll(idle_seconds));
        opts.auto_dismount = opts.idle_timeout.count() > 0;
    }

    const auto vault_path = resolve_vault_path(ctx, args, 0);
    if (vault_path.empty()) {
        return out.error(ErrorCode::UsageError, "mount requires vault PATH");
    }

    auto session = vault::SessionStore::instance().find(vault_path);
    if (!session.is_ok()) {
        CredentialReadOptions cred_options {};
        std::vector<std::filesystem::path> keyfiles;
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (parse_credential_arg(args, i, cred_options)) {
                continue;
            }
            if (args[i] == "--keyfile" && i + 1 < args.size()) {
                keyfiles.emplace_back(args[++i]);
            }
        }

        auto cred_result = read_credentials(ctx.platform, keyfiles, cred_options);
        if (!cred_result.is_ok()) {
            return out.error(cred_result.error(), cred_result.message());
        }
        auto cred = std::move(cred_result.value());

        vault::OpenVaultOptions open_opts {};
        open_opts.protect_hidden = opts.protect_hidden;
        open_opts.read_only = opts.read_only;
        auto opened = vault_engine().open(vault_path, cred, open_opts);
        platform::secure_wipe(cred.password_utf8);
        if (!opened.is_ok()) {
            return out.error(opened.error(), opened.message());
        }
        if (const auto inserted = vault::SessionStore::instance().insert(std::move(opened.value()), true);
            !inserted.is_ok()) {
            return out.error(inserted.error(), inserted.message());
        }
        session = vault::SessionStore::instance().find(vault_path);
        if (!session.is_ok()) {
            return out.error(session.error(), session.message());
        }
    }

    if (opts.protect_hidden) {
        session.value()->protect_hidden = true;
    }
    if (opts.read_only) {
        session.value()->read_only = true;
    }

    auto& manager = mount::MountManager::instance();
    const auto result = manager.mount_vault(vault_path, opts);
    if (!result.is_ok()) {
        return out.error(
            result.error(),
            result.message().empty() ? error_code_name(result.error()) : result.message());
    }

    out.success(
        "Mounted " + vault_path.generic_string() + " at " + mount_point
        + " (session " + std::to_string(result.value()) + ")");

    out.info("Press Ctrl+C to dismount");
    wait_for_mount_shutdown(manager, opts.mount_point);
    return out.success("Dismounted " + mount_point);
#endif
}

} // namespace sv::cli