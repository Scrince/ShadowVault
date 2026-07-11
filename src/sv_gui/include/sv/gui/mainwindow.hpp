#pragma once

#include <sv/gui/vault_service.hpp>

#include <QMainWindow>
#include <QString>

class QAction;
class QLabel;
class QTableWidget;
class QTextEdit;

namespace sv::gui {

/// Main window with volume list, menus, and direct vault engine operations (S1).
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void on_create_volume();
    void on_create_hidden();
    void on_unlock();
    void on_lock();
    void on_verify();
    void on_preferences();
    void on_portable_status();
    void on_mount();
    void on_dismount();
    void on_selection_changed();
    void refresh_volume_list();

private:
    void setup_menus();
    void setup_toolbar();
    void setup_central();
    void setup_status_bar();
    void show_op_result(const QString& action, const VaultOpResult& result);
    [[nodiscard]] QString selected_vault_path() const;

    VaultService vault_service_;
    QTableWidget* volume_table_{nullptr};
    QTextEdit* log_output_{nullptr};
    QLabel* status_label_{nullptr};
    QLabel* portable_label_{nullptr};

    QAction* mount_action_{nullptr};
    QAction* dismount_action_{nullptr};
};

} // namespace sv::gui