#include "user_app_controller.h"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
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
    // 在窗口映射前一次性设定几何:首选 440x820,屏幕放不下时按可用区域缩小并居中
    const auto roots = engine.rootObjects();
    if (auto* window = qobject_cast<QQuickWindow*>(roots.value(0))) {
        const QRect avail = window->screen()->availableGeometry();
        QSize size(qMin(440, avail.width() - 32), qMin(820, avail.height() - 48));
        size = size.expandedTo(QSize(390, qMin(680, qMax(0, avail.height() - 32))));
        window->resize(size);
        window->setPosition(avail.x() + (avail.width() - size.width()) / 2,
                             avail.y() + (avail.height() - size.height()) / 2);
        window->show();
    }
    return app.exec();
}
