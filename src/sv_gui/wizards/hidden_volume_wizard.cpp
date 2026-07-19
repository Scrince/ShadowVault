#include <sv/gui/hidden_volume_wizard.hpp>
#include <sv/gui/vault_service.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace sv::gui {

namespace {

class EducationPage : public QWizardPage {
public:
    explicit EducationPage(QWidget* parent = nullptr)
        : QWizardPage(parent) {
        setTitle(QStringLiteral("Hidden Volume"));
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(
            QStringLiteral(
                "A hidden volume provides plausible deniability within the outer container. "
                "Using the outer volume after creating a hidden volume may overwrite hidden data "
                "unless protection is enabled when unlocking."),
            this));
        understand_check_ = new QCheckBox(QStringLiteral("I understand the risks"), this);
        layout->addWidget(understand_check_);
        registerField(QStringLiteral("understood*"), understand_check_);
    }

private:
    QCheckBox* understand_check_{nullptr};
};

class SizePage : public QWizardPage {
public:
    explicit SizePage(QWidget* parent = nullptr)
        : QWizardPage(parent) {
        setTitle(QStringLiteral("Hidden Size"));
        auto* form = new QFormLayout(this);
        size_spin_ = new QSpinBox(this);
        size_spin_->setRange(32, 1024 * 512);
        size_spin_->setValue(128);
        size_spin_->setSuffix(QStringLiteral(" MiB"));
        form->addRow(QStringLiteral("Hidden volume size:"), size_spin_);
        registerField(QStringLiteral("hiddenSizeMiB"), size_spin_);
    }

private:
    QSpinBox* size_spin_{nullptr};
};

class CredentialsPage : public QWizardPage {
public:
    explicit CredentialsPage(QWidget* parent = nullptr)
        : QWizardPage(parent) {
        setTitle(QStringLiteral("Hidden Credentials"));
        auto* form = new QFormLayout(this);
        password_edit_ = new QLineEdit(this);
        password_edit_->setEchoMode(QLineEdit::Password);
        confirm_edit_ = new QLineEdit(this);
        confirm_edit_->setEchoMode(QLineEdit::Password);
        form->addRow(QStringLiteral("Password:"), password_edit_);
        form->addRow(QStringLiteral("Confirm:"), confirm_edit_);
        registerField(QStringLiteral("hiddenPassword*"), password_edit_);
        registerField(QStringLiteral("hiddenConfirm*"), confirm_edit_);
    }

    bool validatePage() override {
        if (password_edit_->text().size() < 12) {
            QMessageBox::warning(this, QStringLiteral("Hidden Credentials"), QStringLiteral("Password must be at least 12 characters."));
            return false;
        }
        if (password_edit_->text() != confirm_edit_->text()) {
            QMessageBox::warning(this, QStringLiteral("Hidden Credentials"), QStringLiteral("Passwords do not match."));
            return false;
        }
        return true;
    }

private:
    QLineEdit* password_edit_{nullptr};
    QLineEdit* confirm_edit_{nullptr};
};

} 

HiddenVolumeWizard::HiddenVolumeWizard(
    VaultService& service,
    const QString& vault_path,
    QWidget* parent)
    : QWizard(parent)
    , service_(service)
    , vault_path_(vault_path) {
    Q_UNUSED(service_);
    setWindowTitle(QStringLiteral("Create Hidden Volume"));
    setPage(0, new EducationPage(this));
    setPage(1, new SizePage(this));
    setPage(2, new CredentialsPage(this));
}

QString HiddenVolumeWizard::hidden_password() const {
    return field(QStringLiteral("hiddenPassword")).toString();
}

std::uint64_t HiddenVolumeWizard::hidden_size_bytes() const {
    return static_cast<std::uint64_t>(field(QStringLiteral("hiddenSizeMiB")).toInt()) * 1024ULL * 1024ULL;
}

bool HiddenVolumeWizard::understood() const {
    return field(QStringLiteral("understood")).toBool();
}

} 