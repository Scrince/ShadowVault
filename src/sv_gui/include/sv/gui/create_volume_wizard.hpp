#pragma once

#include <QWizard>

namespace sv::gui {

class VaultService;


class CreateVolumeWizard : public QWizard {
    Q_OBJECT

public:
    explicit CreateVolumeWizard(VaultService& service, QWidget* parent = nullptr);

    [[nodiscard]] QString vault_path() const;
    [[nodiscard]] QString password() const;
    [[nodiscard]] std::uint64_t size_bytes() const;
    [[nodiscard]] QString kdf_profile() const;
    [[nodiscard]] bool quick_format() const;

private:
    VaultService& service_;
};

} 