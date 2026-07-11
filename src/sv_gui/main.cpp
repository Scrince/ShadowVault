#include <sv/gui/app.hpp>
#include <sv/gui/mainwindow.hpp>
#include <sv/version.hpp>

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    sv::gui::App::configure(app);

    sv::gui::MainWindow window;
    window.setWindowTitle(
        QStringLiteral("%1 %2").arg(sv::gui::App::kAppName, QString::fromUtf8(SV_VERSION)));
    window.resize(720, 480);
    window.show();

    return QApplication::exec();
}