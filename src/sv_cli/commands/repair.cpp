#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>

#include <sv/container/container_file.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/recovery/repair.hpp>
#include <sv/vault/key_manager.hpp>
#include <sv/platform/terminal_secret.hpp>

#include <cstring>

namespace sv::cli {

namespace {

[[nodiscard]] Result<crypto::SecureBuffer> derive_vmk_for_repair(
    const std::filesystem::path& path,
    const vault::CredentialMaterial& cred) {
    auto container = container::ContainerFileStore::open(path);
    if (!container.is_ok()) {
        return Result<crypto::SecureBuffer>::err(container.error(), container.message());
    }

    const auto primary_sb = container.value().read_superblock_primary();
    if (!primary_sb.is_ok()) {
        return Result<crypto::SecureBuffer>::err(primary_sb.error(), primary_sb.message());
    }

    auto crypto = crypto::create_default_crypto_provider();
    if (!crypto) {
        return Result<crypto::SecureBuffer>::err(ErrorCode::PlatformError, "crypto provider unavailable");
    }

    vault::KeyManager keys(*crypto);
    const auto kdf_params = vault::outer_kdf_params_from_superblock(primary_sb.value());
    const auto derived = keys.derive_from_credentials(
        cred,
        primary_sb.value(),
        vault::HeaderSlotIntent::Outer,
        kdf_params);
    if (!derived.is_ok()) {
        return Result<crypto::SecureBuffer>::err(derived.error(), derived.message());
    }

    const auto primary = container.value().read_header_sector(primary_sb.value().primary_header_offset);
    Result<container::HeaderPayload> opened = Result<container::HeaderPayload>::err(ErrorCode::WrongCredentials, "");
    if (primary.is_ok()) {
        opened = keys.open_header(
            primary.value(),
            derived.value(),
            primary_sb.value(),
            container::HeaderSlotId::PrimaryOuter,
            container::HeaderSlotType::Outer);
    }
    if (!opened.is_ok()) {
        const auto backup = container.value().read_header_sector(primary_sb.value().backup_header_offset);
        if (!backup.is_ok()) {
            return Result<crypto::SecureBuffer>::err(ErrorCode::WrongCredentials, "wrong credentials");
        }
        opened = keys.open_header(
            backup.value(),
            derived.value(),
            primary_sb.value(),
            container::HeaderSlotId::BackupOuter,
            container::HeaderSlotType::Outer);
    }
    if (!opened.is_ok()) {
        return Result<crypto::SecureBuffer>::err(ErrorCode::WrongCredentials, "wrong credentials");
    }

    crypto::SecureBuffer vmk;
    vmk.resize(32);
    std::memcpy(vmk.data(), opened.value().volume_master_key.data(), 32);
    return Result<crypto::SecureBuffer>::ok(std::move(vmk));
}

} // namespace

int repair_command(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    if (args_contain_help(args)) {
        CommandRegistry registry;
        if (const auto* command = registry.find("repair")) {
            registry.print_command_help(output, *command);
            return 0;
        }
    }

    const auto path = resolve_vault_path(ctx, args, 0);
    if (path.empty()) {
        return output.error(ErrorCode::UsageError, "usage: sv repair PATH [--from-backup] [--replay-journal] [--dry-run]");
    }

    recovery::RepairOptions options {};
    CredentialReadOptions cred_options {};
    cred_options.prompt_label = "Password: ";
    std::vector<std::filesystem::path> keyfiles;

    for (std::size_t i = (ctx.globals.vault_path.has_value() ? 0 : 1); i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--dry-run") {
            options.dry_run = true;
            continue;
        }
        if (arg == "--from-backup") {
            options.from_backup = true;
            continue;
        }
        if (arg == "--replay-journal") {
            options.replay_journal = true;
            continue;
        }
        if (arg == "--keyfile" && i + 1 < args.size()) {
            keyfiles.emplace_back(args[++i]);
            continue;
        }
        if (parse_credential_arg(args, i, cred_options)) {
            continue;
        }
    }

    auto container = container::ContainerFileStore::open(path);
    if (!container.is_ok()) {
        return output.error(container.error(), container.message());
    }

    if (options.from_backup && !options.replay_journal) {
        const auto repaired = recovery::repair_from_backup_header(container.value(), options);
        if (!repaired.is_ok()) {
            return output.error(repaired.error(), repaired.message());
        }
        if (options.dry_run) {
            return output.success("repair dry-run: backup header is restorable");
        }
        return output.success("repaired outer primary header from backup");
    }

    auto cred_result = read_credentials(ctx.platform, keyfiles, cred_options);
    if (!cred_result.is_ok()) {
        return output.error(cred_result.error(), cred_result.message());
    }
    auto cred = std::move(cred_result.value());

    const auto vmk = derive_vmk_for_repair(path, cred);
    platform::secure_wipe(cred.password_utf8);
    if (!vmk.is_ok()) {
        return output.error(vmk.error(), vmk.message());
    }

    auto crypto = crypto::create_default_crypto_provider();
    if (!crypto) {
        return output.error(ErrorCode::PlatformError, "crypto provider unavailable");
    }

    const auto repaired = recovery::repair_vault(
        container.value(),
        *crypto,
        vmk.value().span(),
        options);
    if (!repaired.is_ok()) {
        return output.error(repaired.error(), repaired.message());
    }

    if (options.dry_run) {
        if (options.replay_journal) {
            return output.success("repair dry-run: journal replay would run");
        }
        return output.success("repair dry-run: completed checks");
    }

    if (options.replay_journal) {
        return output.success("journal replay completed");
    }

    return output.success("vault repair completed");
}

} // namespace sv::cli