#include <sv/gui/app.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace sv::gui {

void App::configure(QApplication& app) {
    QCoreApplication::setOrganizationName(QString::fromUtf8(kOrgName));
    QCoreApplication::setApplicationName(QString::fromUtf8(kAppName));
    app.setApplicationDisplayName(QString::fromUtf8(kAppName));
}

QString App::resolve_cli_path() {
    const QString sibling = QDir(QCoreApplication::applicationDirPath()).filePath(
        QString::fromUtf8(kCliExecutable) + QStringLiteral(".exe"));
    if (QFileInfo::exists(sibling)) {
        return sibling;
    }
    return QString::fromUtf8(kCliExecutable);
}

} 