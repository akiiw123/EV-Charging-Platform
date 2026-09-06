#include "user_app_controller.h"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

int main(int argc, char* argv[])
{
    QtWebEngineQuick::initialize();
    // 显式固定 Basic 样式:链接 Widgets 后 Qt 会默认选 Fusion,
    // 而 Fusion 需要额外的 QtQuick.Templates QML 插件,部署环境未必安装
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("充电客户端"));
    QCoreApplication::setOrganizationName(QStringLiteral("charging-platform"));

    charging::user::UserAppController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    const QUrl url(QStringLiteral("qrc:/ChargingUser/qml/Main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject* object, const QUrl& objectUrl) {
            if (!object && objectUrl == url) QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    engine.load(url);
    return app.exec();
}
