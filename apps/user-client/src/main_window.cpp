#include "main_window.h"

#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace charging::user {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("充电桩应用管理平台 - 用户端"));
    resize(1100, 720);

    auto* root = new QWidget(this);
    auto* layout = new QVBoxLayout(root);
    auto* title = new QLabel(QStringLiteral("附近充电站"), root);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto* stations = new QListWidget(root);
    stations->addItems({
        QStringLiteral("示例：软件园充电站  |  1.2 km  |  空闲 6/10"),
        QStringLiteral("示例：大学城充电站  |  3.8 km  |  空闲 3/8"),
    });

    layout->addWidget(title);
    layout->addWidget(new QLabel(QStringLiteral("框架页：后续接入定位、腾讯地图、预约充电与订单结算。"), root));
    layout->addWidget(stations, 1);
    setCentralWidget(root);
    statusBar()->showMessage(QStringLiteral("尚未连接管理端"));
}

} // namespace charging::user
