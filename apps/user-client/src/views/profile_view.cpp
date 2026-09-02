#include "profile_view.h"
#include <QDateTime>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
namespace charging::user {
namespace {QString state(const QString&s){if(s==QStringLiteral("completed"))return QStringLiteral("已结算");if(s==QStringLiteral("awaiting_payment"))return QStringLiteral("待结算");if(s==QStringLiteral("charging"))return QStringLiteral("充电中");if(s==QStringLiteral("reserved"))return QStringLiteral("已预约");return QStringLiteral("已取消");}}
ProfileView::ProfileView(QWidget* parent):QWidget(parent)
{
    auto* root=new QVBoxLayout(this);root->setContentsMargins(26,18,26,18);root->setSpacing(12);
    auto* userCard=new QWidget(this);userCard->setObjectName(QStringLiteral("summaryCard"));auto* userRow=new QHBoxLayout(userCard);
    avatar_=new QLabel(QStringLiteral("👤"),userCard);avatar_->setAlignment(Qt::AlignCenter);avatar_->setFixedSize(76,76);avatar_->setStyleSheet(QStringLiteral("font-size:42px;background:#e2e8f0;border-radius:38px;"));
    auto* fields=new QVBoxLayout;nickname_=new QLineEdit(userCard);nickname_->setPlaceholderText(QStringLiteral("编辑昵称"));phone_=new QLabel(userCard);created_=new QLabel(userCard);auto* save=new QPushButton(QStringLiteral("保存昵称"),userCard);
    fields->addWidget(nickname_);fields->addWidget(phone_);fields->addWidget(created_);fields->addWidget(save,0,Qt::AlignLeft);userRow->addWidget(avatar_);userRow->addLayout(fields,1);
    auto* wallet=new QWidget(this);wallet->setObjectName(QStringLiteral("walletCard"));auto* walletRow=new QHBoxLayout(wallet);auto* walletTitle=new QLabel(QStringLiteral("钱包余额"),wallet);balance_=new QLabel(QStringLiteral("￥0.00"),wallet);balance_->setStyleSheet(QStringLiteral("font-size:28px;font-weight:700;color:#0f766e;"));auto* recharge=new QPushButton(QStringLiteral("充值"),wallet);recharge->setObjectName(QStringLiteral("primaryButton"));walletRow->addWidget(walletTitle);walletRow->addWidget(balance_,1);walletRow->addWidget(recharge);
    auto* heading=new QLabel(QStringLiteral("充电订单"),this);heading->setStyleSheet(QStringLiteral("font-size:20px;font-weight:700;color:#17324d;"));auto* scroll=new QScrollArea(this);scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);auto* host=new QWidget;history_=new QVBoxLayout(host);history_->setAlignment(Qt::AlignTop);scroll->setWidget(host);
    auto* logout=new QPushButton(QStringLiteral("退出登录"),this);logout->setObjectName(QStringLiteral("dangerButton"));logout->setMinimumHeight(44);
    root->addWidget(userCard);root->addWidget(wallet);root->addWidget(heading);root->addWidget(scroll,1);root->addWidget(logout);
    connect(save,&QPushButton::clicked,this,[this]{emit nicknameSaveRequested(nickname_->text().trimmed());});
    connect(recharge,&QPushButton::clicked,this,[this]{bool ok=false;double amount=QInputDialog::getDouble(this,QStringLiteral("钱包充值"),QStringLiteral("充值金额（元）"),100,0.01,100000,2,&ok);if(ok)emit rechargeRequested(amount);});
    connect(logout,&QPushButton::clicked,this,[this]{if(QMessageBox::question(this,QStringLiteral("退出登录"),QStringLiteral("确定退出当前账号吗？"))==QMessageBox::Yes)emit logoutRequested();});
}
void ProfileView::setUser(const QJsonObject& u)
{
    nickname_->setText(u.value(QStringLiteral("nickname")).toString());phone_->setText(QStringLiteral("手机号：%1").arg(u.value(QStringLiteral("phone")).toString()));
    const auto dt=QDateTime::fromString(u.value(QStringLiteral("created_at")).toString(),Qt::ISODate);created_->setText(QStringLiteral("注册时间：%1").arg(dt.isValid()?dt.toString(QStringLiteral("yyyy-MM-dd HH:mm")):QStringLiteral("--")));
    balance_->setText(QStringLiteral("￥%1").arg(u.value(QStringLiteral("wallet_balance")).toDouble(),0,'f',2));
}
void ProfileView::setHistory(const QJsonArray& orders)
{
    while(auto* item=history_->takeAt(0)){delete item->widget();delete item;}
    if(orders.isEmpty()){auto* empty=new QLabel(QStringLiteral("暂无充电订单"),this);empty->setAlignment(Qt::AlignCenter);empty->setStyleSheet(QStringLiteral("color:#94a3b8;padding:28px;"));history_->addWidget(empty);return;}
    for(const auto& v:orders){const auto o=v.toObject();auto* card=new QLabel(this);card->setObjectName(QStringLiteral("orderCard"));card->setText(QStringLiteral("订单 #%1    %2\n%3 · %4\n完成时间：%5\n%6 kWh    ￥%7").arg(o.value(QStringLiteral("id")).toInteger()).arg(state(o.value(QStringLiteral("status")).toString()),o.value(QStringLiteral("station_name")).toString(QStringLiteral("充电站")),o.value(QStringLiteral("pile_code")).toString(QStringLiteral("电桩"))).arg(o.value(QStringLiteral("ended_at")).toString().isEmpty()?o.value(QStringLiteral("created_at")).toString():o.value(QStringLiteral("ended_at")).toString()).arg(o.value(QStringLiteral("energy_kwh")).toDouble(),0,'f',3).arg(o.value(QStringLiteral("amount")).toDouble(),0,'f',2));history_->addWidget(card);}
}
}
