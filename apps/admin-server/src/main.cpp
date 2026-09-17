/**
 * @file main.cpp
 * @brief 程序启动入口：初始化 Qt/QML 模块、创建 Controller、注入 QML 上下文并显示主窗口。
 *
 * 调用链说明：QML 调用 Q_INVOKABLE/槽函数，Controller 通过 ApiClient 异步访问服务端，
 * 收到响应后更新 Q_PROPERTY 或列表模型并发出信号，QML 绑定会自动刷新。
 */
#include "admin_app_controller.h"
#include "charging/core/database_manager.h"
#include "charging/core/tcp_server.h"

#include <QDir>
#include <QGuiApplication>
#include <QHostAddress>
#include <QQmlApplicationEngine>
#include <QQmlContext>
extern void qml_register_types_Charging_UI();

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("充电桩运营管理平台"));
    QGuiApplication::setOrganizationName(QStringLiteral("charging-platform"));
    Q_INIT_RESOURCE(qmake_Charging_UI);
    Q_INIT_RESOURCE(charging_ui_raw_qml_0);
    qml_register_types_Charging_UI();
    charging::core::DatabaseManager database;
    QString error;
    const QString databasePath=QDir::current().filePath(QStringLiteral("charging_platform.db"));
    if(!database.open(databasePath,&error)||!database.initialize(&error)){qCritical().noquote()<<QStringLiteral("数据库初始化失败：")<<error;return 1;}
    bool portOk=false;const int configuredPort=qEnvironmentVariableIntValue("CHARGING_SERVER_PORT",&portOk);
    const quint16 port=portOk&&configuredPort>0?quint16(configuredPort):quint16(45454);
    charging::core::TcpServer server(databasePath);
    if(!server.start(QHostAddress::Any,port,&error)){qCritical().noquote()<<QStringLiteral("TCP 服务启动失败：")<<error;return 1;}
    charging::admin::AdminAppController controller(true);
    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.rootContext()->setContextProperty(QStringLiteral("adminController"),&controller);
    QObject::connect(&engine,&QQmlApplicationEngine::objectCreated,&app,[](QObject*object,const QUrl&){if(!object)QCoreApplication::exit(-1);},Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/ChargingAdmin/qml/Main.qml")));
    return app.exec();
}
