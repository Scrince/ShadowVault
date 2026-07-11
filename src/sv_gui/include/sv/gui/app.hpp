#pragma once

#include <QApplication>
#include <QString>

namespace sv::gui {

/// Application metadata and Qt lifecycle helpers for svgui.
class App {
public:
    static constexpr const char* kAppName = "Shadow Vault";
    static constexpr const char* kOrgName = "ShadowVault";
    static constexpr const char* kCliExecutable = "svcli";

    /// Configure organization metadata and application display name.
    static void configure(QApplication& app);

    /// Resolve path to svcli for subprocess delegation (M8 scaffold).
    [[nodiscard]] static QString resolve_cli_path();
};

} // namespace sv::gui