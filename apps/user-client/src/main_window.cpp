#include "main_window.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace charging::user {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
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

    auto* loginLayout = new QHBoxLayout;
    phoneInput_ = new QLineEdit(root);
    phoneInput_->setPlaceholderText(QStringLiteral("输入 11 位手机号"));
    phoneInput_->setMaxLength(11);
    loginButton_ = new QPushButton(QStringLiteral("登录/注册"), root);
    userLabel_ = new QLabel(QStringLiteral("尚未登录"), root);
    loginLayout->addWidget(phoneInput_);
    loginLayout->addWidget(loginButton_);
    loginLayout->addWidget(userLabel_, 1);

    stations_ = new QListWidget(root);
    layout->addWidget(title);
    layout->addLayout(loginLayout);
    layout->addWidget(stations_, 1);
    setCentralWidget(root);

    connect(loginButton_, &QPushButton::clicked, this, &MainWindow::login);
    connect(phoneInput_, &QLineEdit::returnPressed, this, &MainWindow::login);
    connect(&api_, &charging::core::ApiClient::connected, this, [this] {
        statusBar()->showMessage(QStringLiteral("已连接管理端"));
        loginButton_->setEnabled(true);
        loadStations();
    });
    connect(&api_, &charging::core::ApiClient::disconnected, this, [this] {
        statusBar()->showMessage(QStringLiteral("连接已断开，正在重连…"));
        loginButton_->setEnabled(false);
    });
    connect(&api_, &charging::core::ApiClient::clientError, this,
            [this](const QString& error) { statusBar()->showMessage(error, 5000); });
    connect(&api_, &charging::core::ApiClient::responseReceived,
            this, &MainWindow::handleResponse);

    const QString host = qEnvironmentVariable("CHARGING_SERVER_HOST", QStringLiteral("127.0.0.1"));
    bool portOk = false;
    const int configuredPort = qEnvironmentVariableIntValue("CHARGING_SERVER_PORT", &portOk);
    api_.connectToServer(host, portOk && configuredPort > 0 ? quint16(configuredPort) : quint16(45454));
    loginButton_->setEnabled(false);
    statusBar()->showMessage(QStringLiteral("正在连接管理端…"));
}

void MainWindow::login()
{
    api_.send(QStringLiteral("auth.phone_login"),
              {{QStringLiteral("phone"), phoneInput_->text().trimmed()}});
}

void MainWindow::loadStations()
{
    stations_->clear();
    stations_->addItem(QStringLiteral("正在加载…"));
    api_.send(QStringLiteral("station.list"));
}

void MainWindow::handleResponse(const charging::core::Message& message)
{
    if (message.type.endsWith(QStringLiteral(".error"))) {
        statusBar()->showMessage(message.payload.value(QStringLiteral("message")).toString(), 5000);
        return;
    }
    if (message.type == QStringLiteral("auth.phone_login.ok")) {
        const QJsonObject user = message.payload.value(QStringLiteral("user")).toObject();
        userLabel_->setText(QStringLiteral("%1  |  余额 ¥%2")
                                .arg(user.value(QStringLiteral("nickname")).toString())
                                .arg(user.value(QStringLiteral("wallet_balance")).toDouble(), 0, 'f', 2));
        return;
    }
    if (message.type == QStringLiteral("station.list.ok")) {
        stations_->clear();
        const QJsonArray stations = message.payload.value(QStringLiteral("stations")).toArray();
        for (const auto& value : stations) {
            const QJsonObject station = value.toObject();
            stations_->addItem(QStringLiteral("%1  |  ¥%2/度  |  空闲 %3/%4\n%5")
                                   .arg(station.value(QStringLiteral("name")).toString())
                                   .arg(station.value(QStringLiteral("price_per_kwh")).toDouble(), 0, 'f', 2)
                                   .arg(station.value(QStringLiteral("idle_pile_count")).toInt())
                                   .arg(station.value(QStringLiteral("pile_count")).toInt())
                                   .arg(station.value(QStringLiteral("address")).toString()));
        }
        if (stations.isEmpty()) {
            stations_->addItem(QStringLiteral("暂无充电站"));
        }
    }
}

} // namespace charging::user
