#pragma once

#include <QApplication>
#include <QString>

namespace sv::gui {


class App {
public:
    static constexpr const char* kAppName = "Shadow Vault";
    static constexpr const char* kOrgName = "ShadowVault";
    static constexpr const char* kCliExecutable = "svcli";

    
    static void configure(QApplication& app);

    
    [[nodiscard]] static QString resolve_cli_path();
};

} 