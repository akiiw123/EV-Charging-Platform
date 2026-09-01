#include "main_window.h"

#include "charging/core/database_manager.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("充电桩管理端"));
    QApplication::setOrganizationName(QStringLiteral("charging-platform"));

    charging::core::DatabaseManager database;
    QString error;
    const QString databasePath = QDir::current().filePath(QStringLiteral("charging_platform.db"));
    if (!database.open(databasePath, &error) || !database.initialize(&error)) {
        QMessageBox::critical(nullptr, QStringLiteral("数据库初始化失败"), error);
        return 1;
    }

    charging::admin::MainWindow window;
    window.show();
    return app.exec();
}
