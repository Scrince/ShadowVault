#pragma once

#include <QString>
#include <QStringList>

namespace sv::gui {

/// Persistent GUI preferences (QSettings + portable flags).
class GuiSettings {
public:
    static GuiSettings& instance();

    [[nodiscard]] bool portable_mode() const;
    void set_portable_mode(bool enabled);

    [[nodiscard]] bool no_host_traces() const;
    void set_no_host_traces(bool enabled);

    [[nodiscard]] QString kdf_profile() const;
    void set_kdf_profile(const QString& profile);

    [[nodiscard]] QStringList recent_vaults() const;
    void add_recent_vault(const QString& path);

    void sync();

private:
    GuiSettings();

    bool portable_mode_ = false;
    bool no_host_traces_ = false;
    QString kdf_profile_ = QStringLiteral("desktop");
    QStringList recent_vaults_;
};

} // namespace sv::gui