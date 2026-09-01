#include "main_window.h"

#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace charging::user {
namespace {
QString statusText(const QString& value)
{
    if (value == QStringLiteral("reserved")) return QStringLiteral("已预约");
    if (value == QStringLiteral("charging")) return QStringLiteral("充电中");
    if (value == QStringLiteral("awaiting_payment")) return QStringLiteral("待结算");
    if (value == QStringLiteral("completed")) return QStringLiteral("已完成");
    if (value == QStringLiteral("cancelled")) return QStringLiteral("已取消");
    return value;
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("充电桩应用管理平台 - 用户端"));
    resize(1180, 780);
    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);
    auto* loginLayout = new QHBoxLayout;
    phone_ = new QLineEdit(root);
    phone_->setPlaceholderText(QStringLiteral("输入 11 位手机号"));
    phone_->setMaxLength(11);
    loginButton_ = new QPushButton(QStringLiteral("登录/注册"), root);
    userLabel_ = new QLabel(QStringLiteral("尚未登录"), root);
    loginLayout->addWidget(phone_); loginLayout->addWidget(loginButton_); loginLayout->addWidget(userLabel_, 1);
    rootLayout->addLayout(loginLayout);

    auto* tabs = new QTabWidget(root);
    auto* chargingPage = new QWidget(tabs);
    auto* chargingLayout = new QVBoxLayout(chargingPage);
    auto* splitter = new QSplitter(chargingPage);
    stations_ = new QListWidget(splitter);
    piles_ = new QListWidget(splitter);
    stations_->setMinimumWidth(430);
    chargingLayout->addWidget(new QLabel(QStringLiteral("选择充电站和空闲电桩"), chargingPage));
    chargingLayout->addWidget(splitter, 1);
    orderLabel_ = new QLabel(QStringLiteral("当前没有充电订单"), chargingPage);
    chargingLabel_ = new QLabel(chargingPage);
    auto* actions = new QHBoxLayout;
    reserve_ = new QPushButton(QStringLiteral("预约电桩"), chargingPage);
    start_ = new QPushButton(QStringLiteral("开始充电"), chargingPage);
    stop_ = new QPushButton(QStringLiteral("停止充电"), chargingPage);
    settle_ = new QPushButton(QStringLiteral("钱包结算"), chargingPage);
    cancel_ = new QPushButton(QStringLiteral("取消预约"), chargingPage);
    for (auto* button : {reserve_, start_, stop_, settle_, cancel_}) actions->addWidget(button);
    actions->addStretch();
    chargingLayout->addWidget(orderLabel_); chargingLayout->addWidget(chargingLabel_); chargingLayout->addLayout(actions);

    auto* accountPage = new QWidget(tabs);
    auto* accountLayout = new QVBoxLayout(accountPage);
    accountLabel_ = new QLabel(QStringLiteral("请先登录"), accountPage);
    nickname_ = new QLineEdit(accountPage);
    avatar_ = new QLineEdit(accountPage);
    avatar_->setPlaceholderText(QStringLiteral("头像本地路径（可选）"));
    recharge_ = new QLineEdit(accountPage);
    recharge_->setPlaceholderText(QStringLiteral("充值金额"));
    auto* save = new QPushButton(QStringLiteral("保存资料"), accountPage);
    auto* rechargeButton = new QPushButton(QStringLiteral("模拟充值"), accountPage);
    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("昵称"), nickname_);
    form->addRow(QStringLiteral("头像路径"), avatar_);
    form->addRow(save);
    form->addRow(QStringLiteral("充值金额"), recharge_);
    form->addRow(rechargeButton);
    history_ = new QListWidget(accountPage);
    accountLayout->addWidget(accountLabel_); accountLayout->addLayout(form);
    accountLayout->addWidget(new QLabel(QStringLiteral("最近订单"), accountPage)); accountLayout->addWidget(history_, 1);
    tabs->addTab(chargingPage, QStringLiteral("充电服务"));
    tabs->addTab(accountPage, QStringLiteral("我的账户"));
    rootLayout->addWidget(tabs, 1);
    setCentralWidget(root);

    timer_.setInterval(1000);
    connect(&timer_, &QTimer::timeout, this, [this] { ++chargingSeconds_; updateClock(); });
    connect(loginButton_, &QPushButton::clicked, this, &MainWindow::login);
    connect(phone_, &QLineEdit::returnPressed, this, &MainWindow::login);
    connect(stations_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        selectedStationId_ = item->data(Qt::UserRole).toLongLong();
        selectedPrice_ = item->data(Qt::UserRole + 1).toDouble();
        loadPiles(selectedStationId_);
    });
    connect(piles_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        selectedPileId_ = item->data(Qt::UserRole).toLongLong();
        selectedPower_ = item->data(Qt::UserRole + 1).toDouble();
        updateControls();
    });
    connect(reserve_, &QPushButton::clicked, this, [this] {
        api_.send(QStringLiteral("order.reserve"), {{QStringLiteral("pile_id"), selectedPileId_}});
    });
    const auto sendOrder = [this](const QString& type) {
        api_.send(type, {{QStringLiteral("order_id"), order_.value(QStringLiteral("id"))}});
    };
    connect(start_, &QPushButton::clicked, this, [sendOrder] { sendOrder(QStringLiteral("order.start")); });
    connect(stop_, &QPushButton::clicked, this, [sendOrder] { sendOrder(QStringLiteral("order.stop")); });
    connect(settle_, &QPushButton::clicked, this, [sendOrder] { sendOrder(QStringLiteral("order.settle")); });
    connect(cancel_, &QPushButton::clicked, this, [sendOrder] { sendOrder(QStringLiteral("order.cancel")); });
    connect(save, &QPushButton::clicked, this, [this] {
        api_.send(QStringLiteral("user.profile.update"),
                  {{QStringLiteral("nickname"), nickname_->text().trimmed()},
                   {QStringLiteral("avatar_path"), avatar_->text().trimmed()}});
    });
    connect(rechargeButton, &QPushButton::clicked, this, [this] {
        bool ok = false; const double amount = recharge_->text().toDouble(&ok);
        if (!ok) { statusBar()->showMessage(QStringLiteral("请输入有效充值金额"), 5000); return; }
        api_.send(QStringLiteral("wallet.recharge"), {{QStringLiteral("amount"), amount}});
    });
    connect(&api_, &charging::core::ApiClient::connected, this, [this] {
        statusBar()->showMessage(QStringLiteral("已连接管理端")); loginButton_->setEnabled(true); loadStations();
        if (!phone_->text().isEmpty()) login();
    });
    connect(&api_, &charging::core::ApiClient::disconnected, this, [this] {
        statusBar()->showMessage(QStringLiteral("连接已断开，正在重连…")); loginButton_->setEnabled(false);
    });
    connect(&api_, &charging::core::ApiClient::clientError, this,
            [this](const QString& text) { statusBar()->showMessage(text, 5000); });
    connect(&api_, &charging::core::ApiClient::responseReceived, this, &MainWindow::handleResponse);
    const QString host = qEnvironmentVariable("CHARGING_SERVER_HOST", QStringLiteral("127.0.0.1"));
    bool portOk = false; const int configuredPort = qEnvironmentVariableIntValue("CHARGING_SERVER_PORT", &portOk);
    api_.connectToServer(host, portOk && configuredPort > 0 ? quint16(configuredPort) : quint16(45454));
    loginButton_->setEnabled(false); updateClock(); updateControls();
}

void MainWindow::login() { api_.send(QStringLiteral("auth.phone_login"), {{QStringLiteral("phone"), phone_->text().trimmed()}}); }
void MainWindow::loadStations() { stations_->clear(); stations_->addItem(QStringLiteral("正在加载…")); api_.send(QStringLiteral("station.list")); }
void MainWindow::loadPiles(qint64 id) { piles_->clear(); piles_->addItem(QStringLiteral("正在加载…")); api_.send(QStringLiteral("pile.list"), {{QStringLiteral("station_id"), id}}); }
void MainWindow::refreshAccount() { api_.send(QStringLiteral("user.profile")); api_.send(QStringLiteral("order.active")); api_.send(QStringLiteral("order.history")); }

void MainWindow::updateUser(const QJsonObject& value)
{
    user_ = value;
    const QString balance = QString::number(value.value(QStringLiteral("wallet_balance")).toDouble(), 'f', 2);
    userLabel_->setText(value.value(QStringLiteral("nickname")).toString() + QStringLiteral("  |  余额 ¥") + balance);
    accountLabel_->setText(QStringLiteral("手机号：%1    钱包余额：¥%2").arg(value.value(QStringLiteral("phone")).toString(), balance));
    nickname_->setText(value.value(QStringLiteral("nickname")).toString());
    avatar_->setText(value.value(QStringLiteral("avatar_path")).toString());
    updateControls();
}

void MainWindow::updateOrder(const QJsonValue& value)
{
    if (!value.isObject()) {
        order_ = {}; timer_.stop(); chargingSeconds_ = 0;
        orderLabel_->setText(QStringLiteral("当前没有充电订单")); updateClock(); updateControls(); return;
    }
    order_ = value.toObject();
    const QString status = order_.value(QStringLiteral("status")).toString();
    orderLabel_->setText(QStringLiteral("订单 #%1  |  %2  |  电量 %3 kWh  |  金额 ¥%4")
        .arg(order_.value(QStringLiteral("id")).toInteger()).arg(statusText(status))
        .arg(order_.value(QStringLiteral("energy_kwh")).toDouble(), 0, 'f', 3)
        .arg(order_.value(QStringLiteral("amount")).toDouble(), 0, 'f', 2));
    if (status == QStringLiteral("charging")) {
        const QDateTime started = QDateTime::fromString(order_.value(QStringLiteral("started_at")).toString(), Qt::ISODate);
        chargingSeconds_ = started.isValid() ? qMax<qint64>(0, started.secsTo(QDateTime::currentDateTime())) : 0;
        timer_.start();
    } else timer_.stop();
    updateClock(); updateControls();
}

void MainWindow::updateControls()
{
    const QString status = order_.value(QStringLiteral("status")).toString();
    reserve_->setEnabled(!user_.isEmpty() && order_.isEmpty() && selectedPileId_ > 0);
    start_->setEnabled(status == QStringLiteral("reserved"));
    stop_->setEnabled(status == QStringLiteral("charging"));
    settle_->setEnabled(status == QStringLiteral("awaiting_payment"));
    cancel_->setEnabled(status == QStringLiteral("reserved"));
}

void MainWindow::updateClock()
{
    const double energy = selectedPower_ * chargingSeconds_ / 3600.0;
    chargingLabel_->setText(QStringLiteral("计时 %1:%2:%3  |  预计电量 %4 kWh  |  预计费用 ¥%5")
        .arg(chargingSeconds_ / 3600, 2, 10, QLatin1Char('0'))
        .arg((chargingSeconds_ % 3600) / 60, 2, 10, QLatin1Char('0'))
        .arg(chargingSeconds_ % 60, 2, 10, QLatin1Char('0'))
        .arg(energy, 0, 'f', 3).arg(energy * selectedPrice_, 0, 'f', 2));
}

void MainWindow::handleResponse(const charging::core::Message& message)
{
    if (message.type.endsWith(QStringLiteral(".error"))) {
        statusBar()->showMessage(message.payload.value(QStringLiteral("message")).toString(), 7000);
        if (message.payload.value(QStringLiteral("code")) == QStringLiteral("ORDER_ACTIVE_EXISTS")) api_.send(QStringLiteral("order.active"));
        return;
    }
    if (message.type == QStringLiteral("auth.phone_login.ok")) { updateUser(message.payload.value(QStringLiteral("user")).toObject()); refreshAccount(); return; }
    if (message.type == QStringLiteral("user.profile.ok") || message.type == QStringLiteral("user.profile.update.ok") || message.type == QStringLiteral("wallet.recharge.ok")) {
        updateUser(message.payload.value(QStringLiteral("user")).toObject()); recharge_->clear(); return;
    }
    if (message.type == QStringLiteral("station.list.ok")) {
        stations_->clear();
        for (const auto& value : message.payload.value(QStringLiteral("stations")).toArray()) {
            const auto station = value.toObject();
            auto* item = new QListWidgetItem(QStringLiteral("%1  |  ¥%2/度  |  空闲 %3/%4\n%5")
                .arg(station.value(QStringLiteral("name")).toString())
                .arg(station.value(QStringLiteral("price_per_kwh")).toDouble(), 0, 'f', 2)
                .arg(station.value(QStringLiteral("idle_pile_count")).toInt()).arg(station.value(QStringLiteral("pile_count")).toInt())
                .arg(station.value(QStringLiteral("address")).toString()), stations_);
            item->setData(Qt::UserRole, station.value(QStringLiteral("id")).toInteger());
            item->setData(Qt::UserRole + 1, station.value(QStringLiteral("price_per_kwh")).toDouble());
        }
        return;
    }
    if (message.type == QStringLiteral("pile.list.ok")) {
        piles_->clear();
        for (const auto& value : message.payload.value(QStringLiteral("piles")).toArray()) {
            const auto pile = value.toObject(); const QString state = pile.value(QStringLiteral("status")).toString();
            auto* item = new QListWidgetItem(QStringLiteral("%1 | %2 | %3 kW | %4")
                .arg(pile.value(QStringLiteral("code")).toString(), pile.value(QStringLiteral("type")).toString())
                .arg(pile.value(QStringLiteral("power_kw")).toDouble()).arg(state), piles_);
            item->setData(Qt::UserRole, pile.value(QStringLiteral("id")).toInteger());
            item->setData(Qt::UserRole + 1, pile.value(QStringLiteral("power_kw")).toDouble());
            if (state != QStringLiteral("idle")) item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }
        return;
    }
    if (message.type == QStringLiteral("order.active.ok")) { updateOrder(message.payload.value(QStringLiteral("order"))); return; }
    if (message.type == QStringLiteral("order.history.ok")) {
        history_->clear();
        for (const auto& value : message.payload.value(QStringLiteral("orders")).toArray()) {
            const auto item = value.toObject();
            history_->addItem(QStringLiteral("#%1  %2  %3 kWh  ¥%4  %5")
                .arg(item.value(QStringLiteral("id")).toInteger()).arg(statusText(item.value(QStringLiteral("status")).toString()))
                .arg(item.value(QStringLiteral("energy_kwh")).toDouble(), 0, 'f', 3)
                .arg(item.value(QStringLiteral("amount")).toDouble(), 0, 'f', 2).arg(item.value(QStringLiteral("created_at")).toString()));
        }
        return;
    }
    if (message.type.startsWith(QStringLiteral("order."))) {
        if (message.payload.contains(QStringLiteral("user"))) updateUser(message.payload.value(QStringLiteral("user")).toObject());
        const auto updated = message.payload.value(QStringLiteral("order")).toObject();
        const QString state = updated.value(QStringLiteral("status")).toString();
        updateOrder(state == QStringLiteral("completed") || state == QStringLiteral("cancelled") ? QJsonValue(QJsonValue::Null) : QJsonValue(updated));
        api_.send(QStringLiteral("order.history"));
        loadStations();
        if (selectedStationId_ > 0) loadPiles(selectedStationId_);
    }
}

} // namespace charging::user
