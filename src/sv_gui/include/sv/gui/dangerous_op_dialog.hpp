#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

namespace sv::gui {


class DangerousOpDialog : public QDialog {
    Q_OBJECT

public:
    explicit DangerousOpDialog(
        const QString& title,
        const QString& message,
        const QString& confirm_phrase,
        QWidget* parent = nullptr);

    [[nodiscard]] static bool confirm(
        QWidget* parent,
        const QString& title,
        const QString& message,
        const QString& confirm_phrase);

private slots:
    void on_text_changed(const QString& text);

private:
    QString confirm_phrase_;
    QLineEdit* confirm_edit_{nullptr};
    QPushButton* ok_button_{nullptr};
};

} 