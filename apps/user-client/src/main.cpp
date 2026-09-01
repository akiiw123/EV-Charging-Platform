#include "main_window.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("充电桩用户端"));
    QApplication::setOrganizationName(QStringLiteral("charging-platform"));

    charging::user::MainWindow window;
    window.show();
    return app.exec();
}
