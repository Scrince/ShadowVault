#pragma once

#include <QDialog>

namespace sv::gui {

class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
};

} 