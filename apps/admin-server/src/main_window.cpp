#include "main_window.h"

#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>

namespace charging::admin {
namespace {
QTableWidget* makeTable(const QStringList& headers, QWidget* parent)
{
    auto* table = new QTableWidget(0, headers.size(), parent);
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    return table;
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("充电桩应用管理平台 - 运营管理端"));
    resize(1320, 840);
    pages_ = new QStackedWidget(this);

    auto* loginPage = new QWidget(pages_);
    auto* loginLayout = new QVBoxLayout(loginPage);
    auto* title = new QLabel(QStringLiteral("运营管理后台"), loginPage);
    QFont font = title->font(); font.setPointSize(24); font.setBold(true); title->setFont(font);
    username_ = new QLineEdit(QStringLiteral("admin"), loginPage);
    password_ = new QLineEdit(loginPage); password_->setEchoMode(QLineEdit::Password);
    password_->setPlaceholderText(QStringLiteral("默认密码 123456"));
    auto* loginButton = new QPushButton(QStringLiteral("登录"), loginPage);
    loginStatus_ = new QLabel(QStringLiteral("正在连接服务…"), loginPage);
    auto* loginForm = new QFormLayout;
    loginForm->addRow(QStringLiteral("账号"), username_); loginForm->addRow(QStringLiteral("密码"), password_);
    loginLayout->addStretch(); loginLayout->addWidget(title, 0, Qt::AlignHCenter);
    loginLayout->addLayout(loginForm); loginLayout->addWidget(loginButton); loginLayout->addWidget(loginStatus_); loginLayout->addStretch();

    auto* mainPage = new QTabWidget(pages_);
    auto* dashboard = new QWidget(mainPage); auto* dashboardLayout = new QVBoxLayout(dashboard);
    revenueSummary_ = new QLabel(dashboard); pileSummary_ = new QLabel(dashboard);
    revenueChart_ = new QChartView(new QChart, dashboard); revenueChart_->setRenderHint(QPainter::Antialiasing);
    auto* refreshDashboard = new QPushButton(QStringLiteral("刷新总览"), dashboard);
    dashboardLayout->addWidget(revenueSummary_); dashboardLayout->addWidget(pileSummary_);
    dashboardLayout->addWidget(revenueChart_, 1); dashboardLayout->addWidget(refreshDashboard);

    auto* stationsPage = new QWidget(mainPage); auto* stationsLayout = new QVBoxLayout(stationsPage);
    stationTable_ = makeTable({QStringLiteral("ID"),QStringLiteral("名称"),QStringLiteral("地址"),QStringLiteral("纬度"),QStringLiteral("经度"),QStringLiteral("单价"),QStringLiteral("电桩"),QStringLiteral("空闲")}, stationsPage);
    stationName_ = new QLineEdit(stationsPage); stationAddress_ = new QLineEdit(stationsPage);
    latitude_ = new QLineEdit(stationsPage); longitude_ = new QLineEdit(stationsPage);
    price_ = new QLineEdit(stationsPage); pileCount_ = new QLineEdit(stationsPage);
    auto* stationForm = new QFormLayout;
    stationForm->addRow(QStringLiteral("名称"), stationName_); stationForm->addRow(QStringLiteral("地址"), stationAddress_);
    stationForm->addRow(QStringLiteral("纬度"), latitude_); stationForm->addRow(QStringLiteral("经度"), longitude_);
    stationForm->addRow(QStringLiteral("单价"), price_); stationForm->addRow(QStringLiteral("初始电桩数"), pileCount_);
    auto* addStation = new QPushButton(QStringLiteral("新增电站"), stationsPage);
    stationsLayout->addWidget(stationTable_, 1); stationsLayout->addLayout(stationForm); stationsLayout->addWidget(addStation);

    auto* pilesPage = new QWidget(mainPage); auto* pilesLayout = new QVBoxLayout(pilesPage);
    pileTable_ = makeTable({QStringLiteral("ID"),QStringLiteral("编号"),QStringLiteral("电站"),QStringLiteral("类型"),QStringLiteral("功率"),QStringLiteral("状态"),QStringLiteral("次数"),QStringLiteral("时长")}, pilesPage);
    auto* restartPile = new QPushButton(QStringLiteral("远程重启选中电桩"), pilesPage);
    pilesLayout->addWidget(pileTable_, 1); pilesLayout->addWidget(restartPile);

    auto* usersPage = new QWidget(mainPage); auto* usersLayout = new QVBoxLayout(usersPage);
    auto* searchLayout = new QHBoxLayout; userSearch_ = new QLineEdit(usersPage);
    userSearch_->setPlaceholderText(QStringLiteral("按手机号模糊搜索")); auto* search = new QPushButton(QStringLiteral("查询"), usersPage);
    searchLayout->addWidget(userSearch_); searchLayout->addWidget(search);
    userTable_ = makeTable({QStringLiteral("ID"),QStringLiteral("手机号"),QStringLiteral("昵称"),QStringLiteral("余额"),QStringLiteral("状态"),QStringLiteral("注册时间")}, usersPage);
    auto* toggleUser = new QPushButton(QStringLiteral("冻结/解冻选中用户"), usersPage);
    usersLayout->addLayout(searchLayout); usersLayout->addWidget(userTable_, 1); usersLayout->addWidget(toggleUser);

    mainPage->addTab(dashboard, QStringLiteral("运营总览")); mainPage->addTab(stationsPage, QStringLiteral("电站管理"));
    mainPage->addTab(pilesPage, QStringLiteral("电桩管理")); mainPage->addTab(usersPage, QStringLiteral("用户管理"));
    pages_->addWidget(loginPage); pages_->addWidget(mainPage); setCentralWidget(pages_);

    connect(loginButton, &QPushButton::clicked, this, &MainWindow::login);
    connect(password_, &QLineEdit::returnPressed, this, &MainWindow::login);
    connect(refreshDashboard, &QPushButton::clicked, this, [this] { api_.send(QStringLiteral("admin.dashboard")); });
    connect(addStation, &QPushButton::clicked, this, [this] {
        api_.send(QStringLiteral("admin.station.create"), {{QStringLiteral("name"), stationName_->text()},
            {QStringLiteral("address"), stationAddress_->text()}, {QStringLiteral("latitude"), latitude_->text().toDouble()},
            {QStringLiteral("longitude"), longitude_->text().toDouble()}, {QStringLiteral("price_per_kwh"), price_->text().toDouble()},
            {QStringLiteral("pile_count"), pileCount_->text().toInt()}});
    });
    connect(restartPile, &QPushButton::clicked, this, [this] {
        const int row = pileTable_->currentRow(); if (row < 0) return;
        api_.send(QStringLiteral("admin.pile.restart"), {{QStringLiteral("pile_id"), pileTable_->item(row,0)->text().toLongLong()}});
    });
    const auto searchUsers = [this] { api_.send(QStringLiteral("admin.user.list"), {{QStringLiteral("phone"), userSearch_->text()}}); };
    connect(search, &QPushButton::clicked, this, searchUsers); connect(userSearch_, &QLineEdit::returnPressed, this, searchUsers);
    connect(toggleUser, &QPushButton::clicked, this, [this] {
        const int row = userTable_->currentRow(); if (row < 0) return;
        const QString next = userTable_->item(row,4)->text() == QStringLiteral("active") ? QStringLiteral("frozen") : QStringLiteral("active");
        api_.send(QStringLiteral("admin.user.status"), {{QStringLiteral("user_id"), userTable_->item(row,0)->text().toLongLong()}, {QStringLiteral("status"), next}});
    });
    connect(&api_, &charging::core::ApiClient::connected, this, [this] { loginStatus_->setText(QStringLiteral("服务已连接")); });
    connect(&api_, &charging::core::ApiClient::disconnected, this, [this] { pages_->setCurrentIndex(0); loginStatus_->setText(QStringLiteral("连接断开，正在重连…")); });
    connect(&api_, &charging::core::ApiClient::clientError, this, [this](const QString& text) { statusBar()->showMessage(text, 6000); });
    connect(&api_, &charging::core::ApiClient::responseReceived, this, &MainWindow::handleResponse);
    const QString host = qEnvironmentVariable("CHARGING_SERVER_HOST", QStringLiteral("127.0.0.1"));
    bool ok=false; const int configuredPort=qEnvironmentVariableIntValue("CHARGING_SERVER_PORT",&ok);
    api_.connectToServer(host, ok && configuredPort>0 ? quint16(configuredPort) : quint16(45454));
}

void MainWindow::login() { api_.send(QStringLiteral("admin.login"), {{QStringLiteral("username"), username_->text()}, {QStringLiteral("password"), password_->text()}}); }
void MainWindow::refreshAll() { api_.send(QStringLiteral("admin.dashboard")); api_.send(QStringLiteral("admin.station.list")); api_.send(QStringLiteral("admin.pile.list")); api_.send(QStringLiteral("admin.user.list"), {{QStringLiteral("phone"), QString()}}); }

void MainWindow::populateTable(QTableWidget* table, const QJsonArray& rows, const QStringList& keys)
{
    table->setRowCount(rows.size());
    for (int row=0; row<rows.size(); ++row) for (int col=0; col<keys.size(); ++col) {
        const QJsonValue value=rows.at(row).toObject().value(keys.at(col));
        table->setItem(row,col,new QTableWidgetItem(value.isDouble()?QString::number(value.toDouble()):value.toString()));
    }
}

void MainWindow::handleResponse(const charging::core::Message& message)
{
    if (message.type.endsWith(QStringLiteral(".error"))) { statusBar()->showMessage(message.payload.value(QStringLiteral("message")).toString(),7000); return; }
    if (message.type==QStringLiteral("admin.login.ok")) { pages_->setCurrentIndex(1); refreshAll(); return; }
    if (message.type==QStringLiteral("admin.dashboard.ok")) {
        const auto metrics=message.payload.value(QStringLiteral("metrics")).toObject(); const auto status=message.payload.value(QStringLiteral("pile_status")).toObject();
        revenueSummary_->setText(QStringLiteral("今日 ¥%1    本月 ¥%2    累计 ¥%3").arg(metrics.value("today_revenue").toDouble(),0,'f',2).arg(metrics.value("month_revenue").toDouble(),0,'f',2).arg(metrics.value("total_revenue").toDouble(),0,'f',2));
        pileSummary_->setText(QStringLiteral("空闲 %1    充电中 %2    故障 %3    离线 %4").arg(status.value("idle").toInt()).arg(status.value("charging").toInt()).arg(status.value("fault").toInt()).arg(status.value("offline").toInt()));
        auto* series=new QLineSeries; int index=0; for(const auto& value:message.payload.value("revenue_trend").toArray()) series->append(index++,value.toObject().value("amount").toDouble());
        auto* chart=new QChart; chart->addSeries(series); chart->createDefaultAxes(); chart->setTitle(QStringLiteral("近30日营收趋势")); revenueChart_->setChart(chart); return;
    }
    if (message.type==QStringLiteral("admin.station.list.ok")) populateTable(stationTable_,message.payload.value("stations").toArray(),{"id","name","address","latitude","longitude","price_per_kwh","pile_count","idle_pile_count"});
    else if (message.type==QStringLiteral("admin.pile.list.ok")) populateTable(pileTable_,message.payload.value("piles").toArray(),{"id","code","station_name","type","power_kw","status","charge_count","total_charge_minutes"});
    else if (message.type==QStringLiteral("admin.user.list.ok")) populateTable(userTable_,message.payload.value("users").toArray(),{"id","phone","nickname","wallet_balance","status","created_at"});
    else if (message.type==QStringLiteral("admin.station.create.ok")) { stationName_->clear(); stationAddress_->clear(); refreshAll(); }
    else if (message.type==QStringLiteral("admin.pile.restart.ok")) { api_.send(QStringLiteral("admin.pile.list")); api_.send(QStringLiteral("admin.dashboard")); }
    else if (message.type==QStringLiteral("admin.user.status.ok")) api_.send(QStringLiteral("admin.user.list"),{{QStringLiteral("phone"),userSearch_->text()}});
}

} // namespace charging::admin
