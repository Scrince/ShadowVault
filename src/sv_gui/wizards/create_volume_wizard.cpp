#include <sv/gui/create_volume_wizard.hpp>
#include <sv/gui/gui_settings.hpp>
#include <sv/gui/vault_service.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace sv::gui {

namespace {

class LocationPage : public QWizardPage {
public:
    explicit LocationPage(QWidget* parent = nullptr)
        : QWizardPage(parent) {
        setTitle(QStringLiteral("Location & Size"));
        setSubTitle(QStringLiteral("Choose where to store the new .svlt container."));

        auto* layout = new QVBoxLayout(this);
        auto* form = new QFormLayout;

        path_edit_ = new QLineEdit(this);
        auto* browse = new QPushButton(QStringLiteral("Browse…"), this);
        connect(browse, &QPushButton::clicked, this, [this]() {
            const QString path = QFileDialog::getSaveFileName(
                this,
                QStringLiteral("Create vault"),
                path_edit_->text().isEmpty() ? QStringLiteral("vault.svlt") : path_edit_->text(),
                QStringLiteral("Shadow Vault (*.svlt)"));
            if (!path.isEmpty()) {
                path_edit_->setText(path);
            }
        });

        auto* path_row = new QHBoxLayout;
        path_row->addWidget(path_edit_, 1);
        path_row->addWidget(browse);
        form->addRow(QStringLiteral("Vault path:"), path_row);

        size_spin_ = new QSpinBox(this);
        size_spin_->setRange(1, 1024 * 1024);
        size_spin_->setValue(64);
        size_spin_->setSuffix(QStringLiteral(" MiB"));
        form->addRow(QStringLiteral("Container size:"), size_spin_);

        layout->addLayout(form);
        registerField(QStringLiteral("vaultPath*"), path_edit_);
        registerField(QStringLiteral("sizeMiB"), size_spin_);
    }

private:
    QLineEdit* path_edit_{nullptr};
    QSpinBox* size_spin_{nullptr};
};

class SecurityPage : public QWizardPage {
public:
    explicit SecurityPage(QWidget* parent = nullptr)
        : QWizardPage(parent) {
        setTitle(QStringLiteral("Security"));
        setSubTitle(QStringLiteral("Choose a strong password (minimum 12 characters)."));

        auto* form = new QFormLayout(this);
        password_edit_ = new QLineEdit(this);
        password_edit_->setEchoMode(QLineEdit::Password);
        confirm_edit_ = new QLineEdit(this);
        confirm_edit_->setEchoMode(QLineEdit::Password);
        kdf_combo_ = new QComboBox(this);
        kdf_combo_->addItems({QStringLiteral("desktop"), QStringLiteral("portable"), QStringLiteral("paranoid")});
        kdf_combo_->setCurrentText(GuiSettings::instance().kdf_profile());

        form->addRow(QStringLiteral("Password:"), password_edit_);
        form->addRow(QStringLiteral("Confirm:"), confirm_edit_);
        form->addRow(QStringLiteral("KDF profile:"), kdf_combo_);

        registerField(QStringLiteral("password*"), password_edit_);
        registerField(QStringLiteral("confirm*"), confirm_edit_);
        registerField(QStringLiteral("kdfProfile"), kdf_combo_, "currentText");
    }

    bool validatePage() override {
        if (password_edit_->text().size() < 12) {
            QMessageBox::warning(this, QStringLiteral("Security"), QStringLiteral("Password must be at least 12 characters."));
            return false;
        }
        if (password_edit_->text() != confirm_edit_->text()) {
            QMessageBox::warning(this, QStringLiteral("Security"), QStringLiteral("Passwords do not match."));
            return false;
        }
        return true;
    }

private:
    QLineEdit* password_edit_{nullptr};
    QLineEdit* confirm_edit_{nullptr};
    QComboBox* kdf_combo_{nullptr};
};

class OptionsPage : public QWizardPage {
public:
    explicit OptionsPage(QWidget* parent = nullptr)
        : QWizardPage(parent) {
        setTitle(QStringLiteral("Format Options"));
        quick_check_ = new QCheckBox(QStringLiteral("Quick format (sparse allocation)"), this);
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(quick_check_);
        registerField(QStringLiteral("quickFormat"), quick_check_);
    }

private:
    QCheckBox* quick_check_{nullptr};
};

class SummaryPage : public QWizardPage {
public:
    explicit SummaryPage(QWidget* parent = nullptr)
        : QWizardPage(parent) {
        setTitle(QStringLiteral("Summary"));
        summary_label_ = new QLabel(this);
        summary_label_->setWordWrap(true);
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(summary_label_);
    }

    void initializePage() override {
        const auto path = field(QStringLiteral("vaultPath")).toString();
        const auto size = field(QStringLiteral("sizeMiB")).toInt();
        const auto profile = field(QStringLiteral("kdfProfile")).toString();
        const bool quick = field(QStringLiteral("quickFormat")).toBool();
        summary_label_->setText(QStringLiteral(
            "Path: %1\nSize: %2 MiB\nKDF: %3\nQuick format: %4")
                                    .arg(path)
                                    .arg(size)
                                    .arg(profile)
                                    .arg(quick ? QStringLiteral("yes") : QStringLiteral("no")));
    }

private:
    QLabel* summary_label_{nullptr};
};

} // namespace

CreateVolumeWizard::CreateVolumeWizard(VaultService& service, QWidget* parent)
    : QWizard(parent)
    , service_(service) {
    setWindowTitle(QStringLiteral("Create Volume"));
    setPage(0, new LocationPage(this));
    setPage(1, new SecurityPage(this));
    setPage(2, new OptionsPage(this));
    setPage(3, new SummaryPage(this));
}

QString CreateVolumeWizard::vault_path() const {
    return field(QStringLiteral("vaultPath")).toString();
}

QString CreateVolumeWizard::password() const {
    return field(QStringLiteral("password")).toString();
}

std::uint64_t CreateVolumeWizard::size_bytes() const {
    return static_cast<std::uint64_t>(field(QStringLiteral("sizeMiB")).toInt()) * 1024ULL * 1024ULL;
}

QString CreateVolumeWizard::kdf_profile() const {
    return field(QStringLiteral("kdfProfile")).toString();
}

bool CreateVolumeWizard::quick_format() const {
    return field(QStringLiteral("quickFormat")).toBool();
}

} // namespace sv::gui