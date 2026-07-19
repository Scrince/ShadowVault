#include <sv/gui/gui_settings.hpp>

#include <QSettings>

namespace sv::gui {

GuiSettings& GuiSettings::instance() {
    static GuiSettings settings;
    return settings;
}

GuiSettings::GuiSettings() {
    QSettings settings;
    settings.beginGroup(QStringLiteral("portable"));
    portable_mode_ = settings.value(QStringLiteral("enabled"), false).toBool();
    no_host_traces_ = settings.value(QStringLiteral("no_host_traces"), false).toBool();
    settings.endGroup();

    settings.beginGroup(QStringLiteral("security"));
    kdf_profile_ = settings.value(QStringLiteral("kdf_profile"), QStringLiteral("desktop")).toString();
    settings.endGroup();

    recent_vaults_ = settings.value(QStringLiteral("recent_vaults")).toStringList();
}

bool GuiSettings::portable_mode() const {
    return portable_mode_;
}

void GuiSettings::set_portable_mode(const bool enabled) {
    portable_mode_ = enabled;
}

bool GuiSettings::no_host_traces() const {
    return no_host_traces_;
}

void GuiSettings::set_no_host_traces(const bool enabled) {
    no_host_traces_ = enabled;
}

QString GuiSettings::kdf_profile() const {
    return kdf_profile_;
}

void GuiSettings::set_kdf_profile(const QString& profile) {
    kdf_profile_ = profile;
}

QStringList GuiSettings::recent_vaults() const {
    return recent_vaults_;
}

void GuiSettings::add_recent_vault(const QString& path) {
    if (path.isEmpty() || no_host_traces_) {
        return;
    }
    recent_vaults_.removeAll(path);
    recent_vaults_.prepend(path);
    while (recent_vaults_.size() > 10) {
        recent_vaults_.removeLast();
    }
}

void GuiSettings::sync() {
    QSettings settings;
    settings.beginGroup(QStringLiteral("portable"));
    settings.setValue(QStringLiteral("enabled"), portable_mode_);
    settings.setValue(QStringLiteral("no_host_traces"), no_host_traces_);
    settings.endGroup();

    settings.beginGroup(QStringLiteral("security"));
    settings.setValue(QStringLiteral("kdf_profile"), kdf_profile_);
    settings.endGroup();

    if (!no_host_traces_) {
        settings.setValue(QStringLiteral("recent_vaults"), recent_vaults_);
    }
}

} 