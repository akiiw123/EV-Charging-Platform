#include "main_window.h"

#include <QHeaderView>
#include <QLabel>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace charging::admin {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("充电桩应用管理平台 - 运营管理端"));
    resize(1280, 800);

    auto* root = new QWidget(this);
    auto* layout = new QVBoxLayout(root);
    auto* title = new QLabel(QStringLiteral("运营总览"), root);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto* summary = new QLabel(QStringLiteral("今日营收：¥0.00    本月营收：¥0.00    在线电桩：0"), root);
    auto* table = new QTableWidget(0, 5, root);
    table->setHorizontalHeaderLabels({QStringLiteral("电桩编号"), QStringLiteral("所属站点"),
                                      QStringLiteral("类型"), QStringLiteral("功率"), QStringLiteral("状态")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    layout->addWidget(title);
    layout->addWidget(summary);
    layout->addWidget(table, 1);
    setCentralWidget(root);
    statusBar()->showMessage(QStringLiteral("数据库已就绪；TCP 服务待接入"));
}

} // namespace charging::admin
