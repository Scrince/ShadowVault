#include <sv/gui/password_dialog.hpp>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

namespace sv::gui {

PasswordDialog::PasswordDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("Enter Password"));
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    password_edit_ = new QLineEdit(this);
    password_edit_->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("Password:"), password_edit_);

    hidden_check_ = new QCheckBox(QStringLiteral("Unlock hidden volume"), this);
    protect_check_ = new QCheckBox(QStringLiteral("Protect hidden volume (outer)"), this);
    readonly_check_ = new QCheckBox(QStringLiteral("Read-only"), this);

    layout->addLayout(form);
    layout->addWidget(hidden_check_);
    layout->addWidget(protect_check_);
    layout->addWidget(readonly_check_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(hidden_check_, &QCheckBox::toggled, protect_check_, [this](const bool checked) {
        protect_check_->setEnabled(!checked);
        if (checked) {
            protect_check_->setChecked(false);
        }
    });
}

void PasswordDialog::set_offer_hidden_options(const bool enabled) {
    hidden_check_->setVisible(enabled);
    protect_check_->setVisible(enabled);
}

std::optional<PasswordDialogResult> PasswordDialog::get_password(
    QWidget* parent,
    const QString& title,
    const bool offer_hidden_options) {
    PasswordDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.set_offer_hidden_options(offer_hidden_options);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return dialog.result();
}

PasswordDialogResult PasswordDialog::result() const {
    PasswordDialogResult out {};
    out.password = password_edit_->text();
    out.use_hidden = hidden_check_->isChecked();
    out.protect_hidden = protect_check_->isChecked();
    out.read_only = readonly_check_->isChecked();
    return out;
}

} 