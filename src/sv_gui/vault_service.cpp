#include <sv/gui/vault_service.hpp>
#include <sv/gui/gui_settings.hpp>

#include <sv/container/container_file.hpp>
#include <sv/vault/hidden_volume.hpp>
#include <sv/vault/session_store.hpp>

#if defined(SV_HAVE_MOUNT)
#include <sv/mount/mount_manager.hpp>
#endif
#include <sv/vault/vault_engine.hpp>
#include <sv/vault/verify.hpp>

#include <QFileInfo>

namespace sv::gui {

namespace {

std::string to_utf8(const QString& text) {
    return text.toUtf8().constData();
}

vault::CredentialMaterial make_cred(const QString& password) {
    vault::CredentialMaterial cred {};
    cred.password_utf8 = to_utf8(password);
    return cred;
}

void apply_kdf_profile(const QString& profile, vault::CreateVaultOptions& options) {
    if (profile == QStringLiteral("portable")) {
        options.kdf_params = vault::default_kdf_params_portable();
        return;
    }
    if (profile == QStringLiteral("paranoid")) {
        options.kdf_params = vault::default_kdf_params_desktop();
        options.kdf_params.m_cost_kib = 524288;
        options.kdf_params.t_cost = 4;
        return;
    }
    options.kdf_params = vault::default_kdf_params_desktop();
}

VaultOpResult from_result(const sv::Result<void>& result) {
    VaultOpResult out {};
    out.ok = result.is_ok();
    out.message = QString::fromStdString(result.message());
    return out;
}

} 

VaultService::VaultService() = default;

VaultOpResult VaultService::create_vault(
    const QString& path,
    const QString& password,
    const std::uint64_t size_bytes,
    const QString& kdf_profile,
    const bool quick_format) const {
    if (password.size() < 12) {
        return {false, QStringLiteral("Password must be at least 12 characters.")};
    }

    vault::VaultEngine engine;
    auto cred = make_cred(password);

    vault::CreateVaultOptions options {};
    options.container_size = size_bytes;
    options.quick_format = quick_format;
    apply_kdf_profile(kdf_profile, options);

    const auto result = engine.create_new(
        std::filesystem::path(to_utf8(path)),
        cred,
        options);
    return from_result(result);
}

VaultOpResult VaultService::unlock_outer(
    const QString& path,
    const QString& password,
    const bool protect_hidden,
    const bool replace_session) const {
    vault::VaultEngine engine;
    auto cred = make_cred(password);

    vault::OpenVaultOptions options {};
    options.intent = vault::HeaderSlotIntent::Outer;
    options.protect_hidden = protect_hidden;

    auto session = engine.open(std::filesystem::path(to_utf8(path)), cred, options);
    if (!session.is_ok()) {
        return {false, QString::fromStdString(session.message())};
    }

    const auto inserted = vault::SessionStore::instance().insert(
        std::move(session.value()),
        replace_session);
    return from_result(inserted);
}

VaultOpResult VaultService::unlock_hidden(
    const QString& path,
    const QString& password,
    const bool replace_session) const {
    vault::VaultEngine engine;
    auto cred = make_cred(password);

    vault::OpenVaultOptions options {};
    options.intent = vault::HeaderSlotIntent::Hidden;

    auto session = engine.open(std::filesystem::path(to_utf8(path)), cred, options);
    if (!session.is_ok()) {
        return {false, QString::fromStdString(session.message())};
    }

    const auto inserted = vault::SessionStore::instance().insert(
        std::move(session.value()),
        replace_session);
    return from_result(inserted);
}

VaultOpResult VaultService::lock_vault(const QString& path) const {
    vault::VaultEngine engine;
    return from_result(engine.lock(std::filesystem::path(to_utf8(path))));
}

VaultOpResult VaultService::verify_vault(const QString& path, const bool deep) const {
    const auto fs_path = std::filesystem::path(to_utf8(path));
    const auto session = vault::SessionStore::instance().find(fs_path);
    if (session.is_ok()) {
        const auto report = vault::verify_session(*session.value(), deep);
        if (!report.is_ok()) {
            return {false, QString::fromStdString(report.message())};
        }
        return {report.value().structural_ok && (!deep || report.value().crypto_ok),
                QString::fromStdString(report.value().message)};
    }

    auto container = container::ContainerFileStore::open(fs_path);
    if (!container.is_ok()) {
        return {false, QString::fromStdString(container.message())};
    }

    const auto report = vault::verify_structural(container.value());
    if (!report.is_ok()) {
        return {false, QString::fromStdString(report.message())};
    }
    return {report.value().structural_ok, QString::fromStdString(report.value().message)};
}

VaultOpResult VaultService::create_hidden_volume(
    const QString& path,
    const QString& hidden_password,
    const std::uint64_t hidden_size_bytes) const {
    if (hidden_password.size() < 12) {
        return {false, QStringLiteral("Hidden password must be at least 12 characters.")};
    }

    const auto fs_path = std::filesystem::path(to_utf8(path));
    const auto outer = vault::SessionStore::instance().find(fs_path);
    if (!outer.is_ok()) {
        return {false, QStringLiteral("Unlock the outer volume first.")};
    }

    vault::VaultEngine engine;
    auto cred = make_cred(hidden_password);
    const auto created = vault::create_hidden_volume(
        engine,
        *outer.value(),
        cred,
        hidden_size_bytes);
    return from_result(created);
}

void VaultService::register_vault_path(const QString& path) {
    if (path.isEmpty()) {
        return;
    }
    if (!known_paths_.contains(path)) {
        known_paths_.append(path);
    }
    GuiSettings::instance().add_recent_vault(path);
    GuiSettings::instance().sync();
}

VaultOpResult VaultService::mount_vault(
    const QString& path,
    const QString& mount_point,
    const bool read_only,
    const bool protect_hidden) {
#if !defined(SV_HAVE_MOUNT)
    (void)path;
    (void)mount_point;
    (void)read_only;
    (void)protect_hidden;
    return {false, QStringLiteral("Mount requires SV_BUILD_MOUNT=ON")};
#else
    if (mount_point.isEmpty()) {
        return {false, QStringLiteral("Mount point is required.")};
    }

    const auto fs_path = std::filesystem::path(to_utf8(path));
    const auto session = vault::SessionStore::instance().find(fs_path);
    if (!session.is_ok()) {
        return {false, QStringLiteral("Unlock the vault before mounting.")};
    }

    if (protect_hidden) {
        session.value()->protect_hidden = true;
    }
    if (read_only) {
        session.value()->read_only = true;
    }

    mount::MountOptions opts {};
    opts.mount_point = std::filesystem::path(to_utf8(mount_point));
    opts.read_only = read_only;
    opts.protect_hidden = protect_hidden;

    const auto mounted = mount::MountManager::instance().mount_vault(fs_path, opts);
    if (!mounted.is_ok()) {
        return {false, QString::fromStdString(mounted.message())};
    }
    return {true, QStringLiteral("Mounted at %1 (session %2)")
                    .arg(mount_point)
                    .arg(mounted.value())};
#endif
}

VaultOpResult VaultService::dismount_vault(const QString& path) {
#if !defined(SV_HAVE_MOUNT)
    (void)path;
    return {false, QStringLiteral("Mount requires SV_BUILD_MOUNT=ON")};
#else
    const auto fs_path = std::filesystem::path(to_utf8(path));
    auto& manager = mount::MountManager::instance();
    for (const auto& session : manager.list_sessions()) {
        if (session.vault_path == fs_path) {
            return from_result(manager.dismount(session.session_id));
        }
    }
    return {false, QStringLiteral("Vault is not mounted.")};
#endif
}

QList<VaultListEntry> VaultService::list_known_vaults() {
    QStringList paths = known_paths_;
    for (const auto& recent : GuiSettings::instance().recent_vaults()) {
        if (!paths.contains(recent)) {
            paths.append(recent);
        }
    }

    QList<VaultListEntry> entries;
    for (const auto& path : paths) {
        VaultListEntry entry {};
        entry.path = path;
        entry.name = QFileInfo(path).fileName();
        entry.mount_point = QStringLiteral("—");
        entry.status = QStringLiteral("Locked");
        entry.type = QStringLiteral("Outer");

#if defined(SV_HAVE_MOUNT)
        for (const auto& mounted : mount::MountManager::instance().list_sessions()) {
            if (mounted.vault_path == std::filesystem::path(to_utf8(path))) {
                entry.mount_point = QString::fromStdString(mounted.mount_point.generic_string());
                break;
            }
        }
#endif

        const auto session = vault::SessionStore::instance().find(
            std::filesystem::path(to_utf8(path)));
        if (session.is_ok()) {
            entry.status = entry.mount_point == QStringLiteral("—")
                ? QStringLiteral("Unlocked")
                : QStringLiteral("Mounted");
            entry.type = session.value()->hidden_volume
                ? QStringLiteral("Hidden")
                : QStringLiteral("Outer");
        }
        entries.append(entry);
    }
    return entries;
}

} 