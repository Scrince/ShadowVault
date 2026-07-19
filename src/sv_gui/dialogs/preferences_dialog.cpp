#include <sv/gui/preferences_dialog.hpp>
#include <sv/gui/gui_settings.hpp>

#include <QCheckBox>
#include <QLabel>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace sv::gui {

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("Preferences"));
    resize(420, 280);

    auto& settings = GuiSettings::instance();
    auto* tabs = new QTabWidget(this);

    auto* general = new QWidget(tabs);
    auto* general_form = new QFormLayout(general);
    auto* kdf_combo = new QComboBox(general);
    kdf_combo->addItems({QStringLiteral("desktop"), QStringLiteral("portable"), QStringLiteral("paranoid")});
    kdf_combo->setCurrentText(settings.kdf_profile());
    general_form->addRow(QStringLiteral("Default KDF profile:"), kdf_combo);
    tabs->addTab(general, QStringLiteral("General"));

    auto* portable = new QWidget(tabs);
    auto* portable_layout = new QVBoxLayout(portable);
    auto* portable_group = new QGroupBox(QStringLiteral("Portable mode"), portable);
    auto* portable_form = new QFormLayout(portable_group);
    auto* portable_check = new QCheckBox(QStringLiteral("Force portable mode"), portable_group);
    portable_check->setChecked(settings.portable_mode());
    auto* no_traces_check = new QCheckBox(QStringLiteral("No host traces"), portable_group);
    no_traces_check->setChecked(settings.no_host_traces());
    portable_form->addRow(portable_check);
    portable_form->addRow(no_traces_check);
    portable_layout->addWidget(portable_group);
    portable_layout->addStretch(1);
    tabs->addTab(portable, QStringLiteral("Portable"));

    auto* mount = new QWidget(tabs);
    auto* mount_layout = new QVBoxLayout(mount);
    mount_layout->addWidget(new QLabel(
        QStringLiteral("Mount uses WinFsp (Windows) or FUSE (Linux). Enable with SV_BUILD_MOUNT=ON."),
        mount));
    tabs->addTab(mount, QStringLiteral("Mount"));

    auto* root = new QVBoxLayout(this);
    root->addWidget(tabs);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [&, kdf_combo, portable_check, no_traces_check]() {
        settings.set_kdf_profile(kdf_combo->currentText());
        settings.set_portable_mode(portable_check->isChecked());
        settings.set_no_host_traces(no_traces_check->isChecked());
        settings.sync();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

} 