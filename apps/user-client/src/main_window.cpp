#include "main_window.h"
#include "views/charging_view.h"
#include "views/home_view.h"
#include "views/login_view.h"
#include "views/map_navigation_view.h"
#include "views/profile_view.h"
#include "views/station_detail_view.h"
#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace charging::user {
MainWindow::MainWindow(QWidget* parent):QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("充电客户端"));resize(1080,800);setMinimumSize(900,680);buildUi();applyStyle();
    timer_.setInterval(1000);connect(&timer_,&QTimer::timeout,this,[this]{++chargingSeconds_;updateEstimate();});
    connect(&api_,&charging::core::ApiClient::connected,this,[this]{login_->setConnected(true);statusBar()->showMessage(QStringLiteral("已连接充电服务"),3000);});
    connect(&api_,&charging::core::ApiClient::disconnected,this,[this]{login_->setConnected(false);statusBar()->showMessage(QStringLiteral("服务连接断开，正在重连…"));});
    connect(&api_,&charging::core::ApiClient::clientError,this,[this](const QString& e){statusBar()->showMessage(e,6000);});
    connect(&api_,&charging::core::ApiClient::responseReceived,this,&MainWindow::handleResponse);
    const QString host=qEnvironmentVariable("CHARGING_SERVER_HOST",QStringLiteral("127.0.0.1"));bool ok=false;const int port=qEnvironmentVariableIntValue("CHARGING_SERVER_PORT",&ok);api_.connectToServer(host,ok&&port>0?quint16(port):quint16(45454));
}
void MainWindow::buildUi()
{
    root_=new QStackedWidget(this);login_=new LoginView(root_);
    auto* shell=new QWidget(root_);auto* shellLayout=new QVBoxLayout(shell);shellLayout->setContentsMargins(0,0,0,0);shellLayout->setSpacing(0);
    auto* top=new QWidget(shell);top->setObjectName(QStringLiteral("topBar"));top->setFixedHeight(64);auto* topRow=new QHBoxLayout(top);topRow->setContentsMargins(26,0,26,0);
    auto* brand=new QLabel(QStringLiteral("⚡ 充电客户端"),top);
    brand->setStyleSheet(QStringLiteral(
        "font-size:21px;font-weight:700;color:white;background:transparent;"));
    topUser_=new QLabel(top);
    topUser_->setStyleSheet(QStringLiteral("color:white;background:transparent;"));
    topRow->addWidget(brand);topRow->addStretch();topRow->addWidget(topUser_);
    content_=new QStackedWidget(shell);home_=new HomeView(content_);detail_=new StationDetailView(content_);map_=new MapNavigationView(content_);charging_=new ChargingView(content_);profile_=new ProfileView(content_);
    content_->addWidget(home_);content_->addWidget(detail_);content_->addWidget(map_);content_->addWidget(charging_);content_->addWidget(profile_);
    auto* nav=new QWidget(shell);nav->setObjectName(QStringLiteral("bottomNav"));nav->setFixedHeight(72);auto* navRow=new QHBoxLayout(nav);navRow->setContentsMargins(80,8,80,8);navRow->setSpacing(70);
    homeTab_=new QPushButton(QStringLiteral("⌂\n首页"),nav);chargingTab_=new QPushButton(QStringLiteral("⚡\n充电"),nav);profileTab_=new QPushButton(QStringLiteral("●\n我的"),nav);
    for(auto* b:{homeTab_,chargingTab_,profileTab_}){b->setObjectName(QStringLiteral("navButton"));b->setCheckable(true);navRow->addWidget(b,1);}shellLayout->addWidget(top);shellLayout->addWidget(content_,1);shellLayout->addWidget(nav);
    root_->addWidget(login_);root_->addWidget(shell);setCentralWidget(root_);
    connect(login_,&LoginView::loginRequested,this,&MainWindow::login);
    connect(homeTab_,&QPushButton::clicked,this,[this]{showSection(0);});connect(chargingTab_,&QPushButton::clicked,this,[this]{showSection(3);});connect(profileTab_,&QPushButton::clicked,this,[this]{showSection(4);});
    connect(home_,&HomeView::stationSelected,this,[this](const QJsonObject&s){selectedStation_=s;selectedPrice_=s.value(QStringLiteral("price_per_kwh")).toDouble();detail_->setStation(s);content_->setCurrentIndex(1);loadPiles(s.value(QStringLiteral("id")).toInteger());});
    connect(detail_,&StationDetailView::backRequested,this,[this]{showSection(0);});
    connect(detail_,&StationDetailView::reservationRequested,this,[this](qint64 id,double power){selectedPower_=power;api_.send(QStringLiteral("order.reserve"),{{QStringLiteral("pile_id"),id}});});
    connect(detail_,&StationDetailView::navigationRequested,this,[this](const QString& mode,const QJsonObject&s){map_->navigate(mode,home_->latitude(),home_->longitude(),home_->locationName(),s);content_->setCurrentIndex(2);});
    connect(map_,&MapNavigationView::backRequested,this,[this]{content_->setCurrentIndex(1);});
    connect(charging_,&ChargingView::actionRequested,this,[this](const QString& action){if(!order_.isEmpty())api_.send(action,{{QStringLiteral("order_id"),order_.value(QStringLiteral("id"))}});});
    connect(profile_,&ProfileView::nicknameSaveRequested,this,[this](const QString& n){api_.send(QStringLiteral("user.profile.update"),{{QStringLiteral("nickname"),n}});});
    connect(profile_,&ProfileView::rechargeRequested,this,[this](double amount){api_.send(QStringLiteral("wallet.recharge"),{{QStringLiteral("amount"),amount}});});
    connect(profile_,&ProfileView::logoutRequested,this,&MainWindow::logout);
}
void MainWindow::applyStyle()
{
    setStyleSheet(QStringLiteral(
        "QMainWindow,QWidget{background:#f8fafc;font-family:'Noto Sans CJK SC','Microsoft YaHei';font-size:14px;color:#26374a;}"
        "QLineEdit{background:white;border:1px solid #cbd5e1;border-radius:8px;padding:8px 12px;}QLineEdit:focus{border:2px solid #14b8a6;}"
        "QPushButton{background:white;border:1px solid #cbd5e1;border-radius:8px;padding:8px 16px;}QPushButton:hover{border-color:#14b8a6;background:#f0fdfa;}QPushButton:disabled{color:#94a3b8;background:#e2e8f0;}"
        "#primaryButton{background:#0f9f8f;color:white;border:0;font-weight:600;}#secondaryButton{border:1px solid #0f9f8f;color:#0f766e;font-weight:600;}"
        "#dangerButton{background:white;color:#dc2626;border:2px solid #ef4444;font-weight:700;}#textButton{border:0;color:#0f766e;background:transparent;}"
        "#topBar{background:#0f766e;}#bottomNav{background:white;border-top:1px solid #e2e8f0;}#navButton{border:0;background:transparent;color:#64748b;font-weight:600;}#navButton:checked{color:#0f9f8f;background:#ecfdf5;}"
        "#stationCard{text-align:left;background:white;border:1px solid #e2e8f0;border-radius:12px;padding:15px;font-size:15px;}#stationCard:hover{border:2px solid #2dd4bf;}"
        "#summaryCard,#walletCard,#pileCard,#orderCard{background:white;border:1px solid #e2e8f0;border-radius:12px;padding:15px;}QLabel[state='idle']{color:#059669;}QLabel[state='charging']{color:#ea580c;}QLabel[state='fault']{color:#dc2626;}"
    ));
}
void MainWindow::login(const QString& phone)
{
    if(phone.size()!=11){login_->setMessage(QStringLiteral("请输入正确的 11 位手机号"),true);return;}login_->setMessage(QStringLiteral("正在登录…"));api_.send(QStringLiteral("auth.phone_login"),{{QStringLiteral("phone"),phone}});
}
void MainWindow::showSection(int index)
{
    content_->setCurrentIndex(index);homeTab_->setChecked(index==0);chargingTab_->setChecked(index==3);profileTab_->setChecked(index==4);
    if(index==4)refreshAccount();if(index==0)loadStations();
}
void MainWindow::loadStations(){api_.send(QStringLiteral("station.list"));}
void MainWindow::loadPiles(qint64 stationId){api_.send(QStringLiteral("pile.list"),{{QStringLiteral("station_id"),stationId}});}
void MainWindow::refreshAccount(){api_.send(QStringLiteral("user.profile"));api_.send(QStringLiteral("order.active"));api_.send(QStringLiteral("order.history"));}
void MainWindow::updateUser(const QJsonObject& value){user_=value;profile_->setUser(value);topUser_->setText(QStringLiteral("%1  ·  ￥%2").arg(value.value(QStringLiteral("nickname")).toString()).arg(value.value(QStringLiteral("wallet_balance")).toDouble(),0,'f',2));}
void MainWindow::updateOrder(const QJsonValue& value)
{
    order_=value.toObject();charging_->setOrder(value);const QString state=order_.value(QStringLiteral("status")).toString();
    if(state==QStringLiteral("charging")){const auto started=QDateTime::fromString(order_.value(QStringLiteral("started_at")).toString(),Qt::ISODate);chargingSeconds_=started.isValid()?qMax<qint64>(0,started.secsTo(QDateTime::currentDateTime())):0;timer_.start();}else{timer_.stop();chargingSeconds_=0;}updateEstimate();
}
void MainWindow::updateEstimate(){charging_->setEstimate(chargingSeconds_,selectedPower_,selectedPrice_);}
void MainWindow::logout(){timer_.stop();user_={};order_={};topUser_->clear();root_->setCurrentIndex(0);login_->setMessage(QStringLiteral("已安全退出"));}
void MainWindow::handleResponse(const charging::core::Message& m)
{
    if(m.type.endsWith(QStringLiteral(".error"))){const QString msg=m.payload.value(QStringLiteral("message")).toString();statusBar()->showMessage(msg,7000);if(m.type==QStringLiteral("auth.phone_login.error"))login_->setMessage(msg,true);if(m.payload.value(QStringLiteral("code")).toString()==QStringLiteral("ORDER_ACTIVE_EXISTS")){api_.send(QStringLiteral("order.active"));showSection(3);}return;}
    if(m.type==QStringLiteral("auth.phone_login.ok")){updateUser(m.payload.value(QStringLiteral("user")).toObject());root_->setCurrentIndex(1);showSection(0);refreshAccount();return;}
    if(m.type==QStringLiteral("station.list.ok")){stations_=m.payload.value(QStringLiteral("stations")).toArray();home_->setStations(stations_);return;}
    if(m.type==QStringLiteral("pile.list.ok")){piles_=m.payload.value(QStringLiteral("piles")).toArray();detail_->setPiles(piles_,order_.isEmpty());return;}
    if(m.type==QStringLiteral("user.profile.ok")||m.type==QStringLiteral("user.profile.update.ok")||m.type==QStringLiteral("wallet.recharge.ok")){updateUser(m.payload.value(QStringLiteral("user")).toObject());return;}
    if(m.type==QStringLiteral("order.active.ok")){updateOrder(m.payload.value(QStringLiteral("order")));return;}
    if(m.type==QStringLiteral("order.history.ok")){profile_->setHistory(m.payload.value(QStringLiteral("orders")).toArray());return;}
    if(m.type.startsWith(QStringLiteral("order."))){if(m.payload.contains(QStringLiteral("user")))updateUser(m.payload.value(QStringLiteral("user")).toObject());const auto updated=m.payload.value(QStringLiteral("order")).toObject();const QString s=updated.value(QStringLiteral("status")).toString();updateOrder(s==QStringLiteral("completed")||s==QStringLiteral("cancelled")?QJsonValue():QJsonValue(updated));api_.send(QStringLiteral("order.history"));loadStations();if(!selectedStation_.isEmpty())loadPiles(selectedStation_.value(QStringLiteral("id")).toInteger());showSection(3);}
}
}
