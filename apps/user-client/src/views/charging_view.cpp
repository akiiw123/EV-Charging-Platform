#include "charging_view.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
namespace charging::user {
namespace {QString text(const QString&s){if(s==QStringLiteral("reserved"))return QStringLiteral("已预约");if(s==QStringLiteral("charging"))return QStringLiteral("充电中");if(s==QStringLiteral("awaiting_payment"))return QStringLiteral("待结算");return QStringLiteral("暂无订单");}}
ChargingView::ChargingView(QWidget* parent):QWidget(parent)
{
    auto* root=new QVBoxLayout(this);root->setContentsMargins(50,35,50,35);root->setSpacing(18);
    auto* title=new QLabel(QStringLiteral("充电中心"),this);title->setStyleSheet(QStringLiteral("font-size:26px;font-weight:700;color:#17324d;"));
    state_=new QLabel(QStringLiteral("当前没有进行中的充电订单"),this);state_->setObjectName(QStringLiteral("summaryCard"));state_->setAlignment(Qt::AlignCenter);state_->setMinimumHeight(170);
    estimate_=new QLabel(QStringLiteral("请在首页选择电站并预约空闲电桩"),this);estimate_->setAlignment(Qt::AlignCenter);estimate_->setStyleSheet(QStringLiteral("font-size:16px;color:#64748b;"));
    start_=new QPushButton(QStringLiteral("开始充电"),this);stop_=new QPushButton(QStringLiteral("停止充电"),this);settle_=new QPushButton(QStringLiteral("钱包结算"),this);cancel_=new QPushButton(QStringLiteral("取消预约"),this);
    start_->setObjectName(QStringLiteral("primaryButton"));settle_->setObjectName(QStringLiteral("primaryButton"));
    for(auto* b:{start_,stop_,settle_,cancel_}){b->setMinimumHeight(44);root->addWidget(b);}
    root->insertWidget(0,title);root->insertWidget(1,state_,1);root->insertWidget(2,estimate_);root->addStretch();
    connect(start_,&QPushButton::clicked,this,[this]{emit actionRequested(QStringLiteral("order.start"));});
    connect(stop_,&QPushButton::clicked,this,[this]{emit actionRequested(QStringLiteral("order.stop"));});
    connect(settle_,&QPushButton::clicked,this,[this]{emit actionRequested(QStringLiteral("order.settle"));});
    connect(cancel_,&QPushButton::clicked,this,[this]{emit actionRequested(QStringLiteral("order.cancel"));});
    setOrder(QJsonValue());
}
void ChargingView::setOrder(const QJsonValue& value)
{
    const QJsonObject order=value.toObject();const QString status=order.value(QStringLiteral("status")).toString();
    if(order.isEmpty())state_->setText(QStringLiteral("当前没有进行中的充电订单"));
    else state_->setText(QStringLiteral("订单 #%1\n%2\n电量 %3 kWh    金额 ￥%4").arg(order.value(QStringLiteral("id")).toInteger()).arg(text(status)).arg(order.value(QStringLiteral("energy_kwh")).toDouble(),0,'f',3).arg(order.value(QStringLiteral("amount")).toDouble(),0,'f',2));
    start_->setVisible(status==QStringLiteral("reserved"));cancel_->setVisible(status==QStringLiteral("reserved"));stop_->setVisible(status==QStringLiteral("charging"));settle_->setVisible(status==QStringLiteral("awaiting_payment"));
}
void ChargingView::setEstimate(qint64 s,double power,double price)
{
    const double energy=power*s/3600.0;estimate_->setText(QStringLiteral("计时 %1:%2:%3  ·  预计 %4 kWh  ·  ￥%5").arg(s/3600,2,10,QLatin1Char('0')).arg((s%3600)/60,2,10,QLatin1Char('0')).arg(s%60,2,10,QLatin1Char('0')).arg(energy,0,'f',3).arg(energy*price,0,'f',2));
}
}
