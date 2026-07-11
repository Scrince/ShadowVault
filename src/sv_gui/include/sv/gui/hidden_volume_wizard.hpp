#pragma once

#include <QWizard>

namespace sv::gui {

class VaultService;

/// Hidden volume creation wizard (S3).
class HiddenVolumeWizard : public QWizard {
    Q_OBJECT

public:
    explicit HiddenVolumeWizard(VaultService& service, const QString& vault_path, QWidget* parent = nullptr);

    [[nodiscard]] QString hidden_password() const;
    [[nodiscard]] std::uint64_t hidden_size_bytes() const;
    [[nodiscard]] bool understood() const;

private:
    VaultService& service_;
    QString vault_path_;
};

} // namespace sv::gui