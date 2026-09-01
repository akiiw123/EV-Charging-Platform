#include "main_window.h"

#include "charging/core/database_manager.h"
#include "charging/core/tcp_server.h"

#include <QApplication>
#include <QDir>
#include <QHostAddress>
#include <QMessageBox>
#include <QStatusBar>

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

    bool portOk = false;
    const quint16 configuredPort = qEnvironmentVariableIntValue("CHARGING_SERVER_PORT", &portOk);
    const quint16 port = portOk && configuredPort > 0 ? configuredPort : 45454;
    charging::core::TcpServer server(databasePath);
    if (!server.start(QHostAddress::Any, port, &error)) {
        QMessageBox::critical(nullptr, QStringLiteral("服务启动失败"), error);
        return 1;
    }

    charging::admin::MainWindow window;
    window.statusBar()->showMessage(QStringLiteral("数据库已就绪；TCP 服务监听端口 %1").arg(port));
    window.show();
    return app.exec();
}
