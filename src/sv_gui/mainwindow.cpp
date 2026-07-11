#include <sv/gui/app.hpp>
#include <sv/gui/create_volume_wizard.hpp>
#include <sv/gui/dangerous_op_dialog.hpp>
#include <sv/gui/gui_settings.hpp>
#include <sv/gui/hidden_volume_wizard.hpp>
#include <sv/gui/mainwindow.hpp>
#include <sv/gui/password_dialog.hpp>
#include <sv/gui/preferences_dialog.hpp>
#include <sv/gui/vault_service.hpp>

#include <sv/platform/platform_context.hpp>
#include <sv/portable/portable_config.hpp>

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

namespace sv::gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setup_menus();
    setup_toolbar();
    setup_central();
    setup_status_bar();
    refresh_volume_list();
}

void MainWindow::setup_menus() {
    auto* file_menu = menuBar()->addMenu(QStringLiteral("&File"));
    file_menu->addAction(QStringLiteral("E&xit"), this, &QWidget::close);

    auto* volumes_menu = menuBar()->addMenu(QStringLiteral("&Volumes"));
    mount_action_ = volumes_menu->addAction(QStringLiteral("&Mount…"), this, &MainWindow::on_mount);
    dismount_action_ = volumes_menu->addAction(QStringLiteral("&Dismount"), this, &MainWindow::on_dismount);
    volumes_menu->addSeparator();
    volumes_menu->addAction(QStringLiteral("Create &New Volume…"), this, &MainWindow::on_create_volume);
    volumes_menu->addAction(QStringLiteral("Create &Hidden Volume…"), this, &MainWindow::on_create_hidden);
    volumes_menu->addSeparator();
    volumes_menu->addAction(QStringLiteral("&Verify"), this, &MainWindow::on_verify);

    auto* tools_menu = menuBar()->addMenu(QStringLiteral("&Tools"));
    tools_menu->addAction(QStringLiteral("&Preferences…"), this, &MainWindow::on_preferences);

    auto* portable_menu = menuBar()->addMenu(QStringLiteral("P&ortable"));
    portable_menu->addAction(QStringLiteral("Portable &Status…"), this, &MainWindow::on_portable_status);

    auto* help_menu = menuBar()->addMenu(QStringLiteral("&Help"));
    help_menu->addAction(QStringLiteral("&About"), this, [this]() {
        QMessageBox::about(
            this,
            QStringLiteral("About Shadow Vault"),
            QStringLiteral("Shadow Vault GUI (svgui)\nWinFsp/FUSE mount when SV_BUILD_MOUNT=ON."));
    });
}

void MainWindow::setup_toolbar() {
    auto* toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->addAction(QStringLiteral("Create"), this, &MainWindow::on_create_volume);
    toolbar->addAction(QStringLiteral("Unlock"), this, &MainWindow::on_unlock);
    toolbar->addAction(QStringLiteral("Lock"), this, &MainWindow::on_lock);
    toolbar->addAction(QStringLiteral("Verify"), this, &MainWindow::on_verify);
    toolbar->addSeparator();
    toolbar->addAction(QStringLiteral("Mount"), this, &MainWindow::on_mount);
    toolbar->addAction(QStringLiteral("Dismount"), this, &MainWindow::on_dismount);
    toolbar->addAction(QStringLiteral("Preferences"), this, &MainWindow::on_preferences);
}

void MainWindow::setup_central() {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    volume_table_ = new QTableWidget(0, 5, central);
    volume_table_->setHorizontalHeaderLabels({
        QStringLiteral("Name"),
        QStringLiteral("Path"),
        QStringLiteral("Status"),
        QStringLiteral("Mount"),
        QStringLiteral("Type"),
    });
    volume_table_->horizontalHeader()->setStretchLastSection(true);
    volume_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    volume_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(volume_table_, &QTableWidget::itemSelectionChanged, this, &MainWindow::on_selection_changed);
    layout->addWidget(volume_table_, 2);

    log_output_ = new QTextEdit(central);
    log_output_->setReadOnly(true);
    layout->addWidget(log_output_, 1);

    setCentralWidget(central);
}

void MainWindow::setup_status_bar() {
    portable_label_ = new QLabel(this);
    status_label_ = new QLabel(QStringLiteral("Ready"), this);
    statusBar()->addWidget(portable_label_, 1);
    statusBar()->addPermanentWidget(status_label_);

    const auto ctx = platform::create_platform_context();
    const auto status = portable::portable_status(ctx);
    const bool portable = status.portable_mode_active || GuiSettings::instance().portable_mode();
    portable_label_->setText(
        portable ? QStringLiteral("Portable mode active")
                 : QStringLiteral("Host mode"));
}

void MainWindow::refresh_volume_list() {
    const auto entries = vault_service_.list_known_vaults();
    volume_table_->setRowCount(entries.size());
    for (int row = 0; row < entries.size(); ++row) {
        const auto& entry = entries[row];
        volume_table_->setItem(row, 0, new QTableWidgetItem(entry.name));
        volume_table_->setItem(row, 1, new QTableWidgetItem(
            GuiSettings::instance().no_host_traces() ? QStringLiteral("—") : entry.path));
        volume_table_->setItem(row, 2, new QTableWidgetItem(entry.status));
        volume_table_->setItem(row, 3, new QTableWidgetItem(entry.mount_point));
        volume_table_->setItem(row, 4, new QTableWidgetItem(entry.type));
        volume_table_->item(row, 0)->setData(Qt::UserRole, entry.path);
    }
}

void MainWindow::on_selection_changed() {
    Q_UNUSED(selected_vault_path());
}

QString MainWindow::selected_vault_path() const {
    const auto items = volume_table_->selectedItems();
    if (items.isEmpty()) {
        return {};
    }
    return items.front()->data(Qt::UserRole).toString();
}

void MainWindow::show_op_result(const QString& action, const VaultOpResult& result) {
    log_output_->append(QStringLiteral("[%1] %2").arg(action, result.message));
    status_label_->setText(result.ok ? QStringLiteral("Ready") : QStringLiteral("Error"));
    if (!result.ok) {
        QMessageBox::warning(this, action, result.message);
    } else {
        refresh_volume_list();
    }
}

void MainWindow::on_create_volume() {
    CreateVolumeWizard wizard(vault_service_, this);
    if (wizard.exec() != QDialog::Accepted) {
        return;
    }

    const QString path = wizard.vault_path();
    if (QFileInfo::exists(path)) {
        if (!DangerousOpDialog::confirm(
                this,
                QStringLiteral("Overwrite vault?"),
                QStringLiteral("An existing file will be overwritten."),
                QStringLiteral("OVERWRITE"))) {
            return;
        }
    }

    QProgressDialog progress(QStringLiteral("Creating vault…"), QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();
    QApplication::processEvents();

    const auto result = vault_service_.create_vault(
        path,
        wizard.password(),
        wizard.size_bytes(),
        wizard.kdf_profile(),
        wizard.quick_format());
    progress.close();

    if (result.ok) {
        vault_service_.register_vault_path(path);
    }
    show_op_result(QStringLiteral("Create"), result);
}

void MainWindow::on_create_hidden() {
    const QString path = selected_vault_path();
    if (path.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Hidden Volume"), QStringLiteral("Select a vault first."));
        return;
    }

    HiddenVolumeWizard wizard(vault_service_, path, this);
    if (wizard.exec() != QDialog::Accepted || !wizard.understood()) {
        return;
    }

    if (!DangerousOpDialog::confirm(
            this,
            QStringLiteral("Create hidden volume"),
            QStringLiteral("This allocates hidden space inside the outer volume. Outer writes can destroy hidden data without protection."),
            QStringLiteral("HIDDEN"))) {
        return;
    }

    QProgressDialog progress(QStringLiteral("Creating hidden volume…"), QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();
    QApplication::processEvents();

    const auto result = vault_service_.create_hidden_volume(
        path,
        wizard.hidden_password(),
        wizard.hidden_size_bytes());
    progress.close();
    show_op_result(QStringLiteral("Create Hidden"), result);
}

void MainWindow::on_unlock() {
    QString path = selected_vault_path();
    if (path.isEmpty()) {
        path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Select vault"),
            {},
            QStringLiteral("Shadow Vault (*.svlt)"));
        if (path.isEmpty()) {
            return;
        }
        vault_service_.register_vault_path(path);
    }

    const auto cred = PasswordDialog::get_password(this, QStringLiteral("Unlock Vault"), true);
    if (!cred.has_value()) {
        return;
    }

    VaultOpResult result {};
    if (cred->use_hidden) {
        result = vault_service_.unlock_hidden(path, cred->password, true);
    } else {
        result = vault_service_.unlock_outer(path, cred->password, cred->protect_hidden, true);
    }
    show_op_result(QStringLiteral("Unlock"), result);
}

void MainWindow::on_lock() {
    const QString path = selected_vault_path();
    if (path.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Lock"), QStringLiteral("Select a vault first."));
        return;
    }
    show_op_result(QStringLiteral("Lock"), vault_service_.lock_vault(path));
}

void MainWindow::on_verify() {
    const QString path = selected_vault_path();
    if (path.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Verify"), QStringLiteral("Select a vault first."));
        return;
    }

    QProgressDialog progress(QStringLiteral("Verifying…"), QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();
    QApplication::processEvents();

    const auto result = vault_service_.verify_vault(path, true);
    progress.close();
    show_op_result(QStringLiteral("Verify"), result);
}

void MainWindow::on_preferences() {
    PreferencesDialog dialog(this);
    dialog.exec();
    setup_status_bar();
}

void MainWindow::on_portable_status() {
    const auto ctx = platform::create_platform_context();
    const auto status = portable::portable_status(ctx);
    QStringList lines;
    lines << QStringLiteral("Portable: %1").arg(status.portable_mode_active ? QStringLiteral("yes") : QStringLiteral("no"));
    lines << QStringLiteral("No host traces: %1").arg(status.no_host_traces ? QStringLiteral("yes") : QStringLiteral("no"));
    lines << QStringLiteral("Config: %1").arg(status.config_path.generic_string().c_str());
    lines << QStringLiteral("Temp pending: %1").arg(status.temp_files_pending);
    QMessageBox::information(this, QStringLiteral("Portable Status"), lines.join(QLatin1Char('\n')));
}

void MainWindow::on_mount() {
    QString path = selected_vault_path();
    if (path.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Mount"), QStringLiteral("Select a vault first."));
        return;
    }

    bool ok = false;
    const QString mount_point = QInputDialog::getText(
        this,
        QStringLiteral("Mount Volume"),
        QStringLiteral("Mount point (e.g. X: or C:\\mount\\vault):"),
        QLineEdit::Normal,
        QStringLiteral("X:"),
        &ok);
    if (!ok || mount_point.isEmpty()) {
        return;
    }

    const auto read_only = QMessageBox::question(
        this,
        QStringLiteral("Mount Options"),
        QStringLiteral("Mount as read-only?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No) == QMessageBox::Yes;

    show_op_result(
        QStringLiteral("Mount"),
        vault_service_.mount_vault(path, mount_point, read_only, false));
}

void MainWindow::on_dismount() {
    const QString path = selected_vault_path();
    if (path.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Dismount"), QStringLiteral("Select a vault first."));
        return;
    }
    show_op_result(QStringLiteral("Dismount"), vault_service_.dismount_vault(path));
}

} // namespace sv::gui