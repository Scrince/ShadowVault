#pragma once

#include <QDialog>
#include <optional>

class QCheckBox;
class QLineEdit;

namespace sv::gui {

struct PasswordDialogResult {
    QString password;
    bool use_hidden = false;
    bool protect_hidden = false;
    bool read_only = false;
};


class PasswordDialog : public QDialog {
    Q_OBJECT

public:
    explicit PasswordDialog(QWidget* parent = nullptr);

    [[nodiscard]] static std::optional<PasswordDialogResult> get_password(
        QWidget* parent,
        const QString& title,
        bool offer_hidden_options = true);

    [[nodiscard]] PasswordDialogResult result() const;

    void set_offer_hidden_options(bool enabled);

private:
    QLineEdit* password_edit_{nullptr};
    QCheckBox* hidden_check_{nullptr};
    QCheckBox* protect_check_{nullptr};
    QCheckBox* readonly_check_{nullptr};
};

} 