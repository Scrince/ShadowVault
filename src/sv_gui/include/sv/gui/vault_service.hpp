#pragma once

#include <QList>
#include <QString>

#include <cstdint>

namespace sv::gui {

struct VaultOpResult {
    bool ok = false;
    QString message;
};

struct VaultListEntry {
    QString path;
    QString name;
    QString status;
    QString mount_point;
    QString type;
};

/// Direct vault engine access for svgui (no subprocess).
class VaultService {
public:
    VaultService();

    [[nodiscard]] VaultOpResult create_vault(
        const QString& path,
        const QString& password,
        std::uint64_t size_bytes,
        const QString& kdf_profile,
        bool quick_format) const;

    [[nodiscard]] VaultOpResult unlock_outer(
        const QString& path,
        const QString& password,
        bool protect_hidden,
        bool replace_session) const;

    [[nodiscard]] VaultOpResult unlock_hidden(
        const QString& path,
        const QString& password,
        bool replace_session) const;

    [[nodiscard]] VaultOpResult lock_vault(const QString& path) const;

    [[nodiscard]] VaultOpResult verify_vault(const QString& path, bool deep) const;

    [[nodiscard]] VaultOpResult create_hidden_volume(
        const QString& path,
        const QString& hidden_password,
        std::uint64_t hidden_size_bytes) const;

    [[nodiscard]] QList<VaultListEntry> list_known_vaults();

    [[nodiscard]] VaultOpResult mount_vault(
        const QString& path,
        const QString& mount_point,
        bool read_only = false,
        bool protect_hidden = false);

    [[nodiscard]] VaultOpResult dismount_vault(const QString& path);

    void register_vault_path(const QString& path);

private:
    QStringList known_paths_;
};

} // namespace sv::gui