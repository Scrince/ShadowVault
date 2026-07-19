#include <sv/gui/dangerous_op_dialog.hpp>

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>

namespace sv::gui {

DangerousOpDialog::DangerousOpDialog(
    const QString& title,
    const QString& message,
    const QString& confirm_phrase,
    QWidget* parent)
    : QDialog(parent)
    , confirm_phrase_(confirm_phrase) {
    setWindowTitle(title);
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    auto* message_label = new QLabel(message, this);
    message_label->setWordWrap(true);
    layout->addWidget(message_label);

    auto* prompt = new QLabel(
        QStringLiteral("Type %1 to confirm:").arg(confirm_phrase),
        this);
    layout->addWidget(prompt);

    confirm_edit_ = new QLineEdit(this);
    layout->addWidget(confirm_edit_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    ok_button_ = buttons->button(QDialogButtonBox::Ok);
    ok_button_->setEnabled(false);
    layout->addWidget(buttons);

    connect(confirm_edit_, &QLineEdit::textChanged, this, &DangerousOpDialog::on_text_changed);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

bool DangerousOpDialog::confirm(
    QWidget* parent,
    const QString& title,
    const QString& message,
    const QString& confirm_phrase) {
    DangerousOpDialog dialog(title, message, confirm_phrase, parent);
    return dialog.exec() == QDialog::Accepted;
}

void DangerousOpDialog::on_text_changed(const QString& text) {
    ok_button_->setEnabled(text == confirm_phrase_);
}

} 