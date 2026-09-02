#include "home_view.h"
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QtMath>
#include <algorithm>

namespace charging::user {
HomeView::HomeView(QWidget* parent) : QWidget(parent)
{
    auto* root = new QVBoxLayout(this); root->setContentsMargins(26,20,26,18); root->setSpacing(14);
    auto* locationRow = new QHBoxLayout;
    location_ = new QLabel(QStringLiteral("📍 深圳市"), this); location_->setStyleSheet(QStringLiteral("font-size:18px;font-weight:700;color:#17324d;"));
    search_ = new QLineEdit(this); search_->setPlaceholderText(QStringLiteral("输入城市、地址或充电站名称")); search_->setMinimumHeight(40);
    auto* locateButton = new QPushButton(QStringLiteral("定位"), this); locateButton->setMinimumHeight(40);
    locationRow->addWidget(location_); locationRow->addWidget(search_,1); locationRow->addWidget(locateButton);
    auto* heading = new QLabel(QStringLiteral("附近充电站"), this); heading->setStyleSheet(QStringLiteral("font-size:22px;font-weight:700;color:#17324d;"));
    auto* scroll = new QScrollArea(this); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto* host = new QWidget(scroll); cards_ = new QVBoxLayout(host); cards_->setAlignment(Qt::AlignTop); cards_->setSpacing(12); scroll->setWidget(host);
    root->addLayout(locationRow); root->addWidget(heading); root->addWidget(scroll,1);
    connect(locateButton, &QPushButton::clicked, this, &HomeView::locate);
    connect(search_, &QLineEdit::returnPressed, this, &HomeView::locate);
}
QString HomeView::locationName() const { return location_->text().mid(2).trimmed(); }
void HomeView::setStations(const QJsonArray& stations) { stations_ = stations; render(); }
double HomeView::distanceKm(double a,double b,double c,double d)
{
    constexpr double radius=6371.0; const double dLat=qDegreesToRadians(c-a), dLon=qDegreesToRadians(d-b);
    const double value=qSin(dLat/2)*qSin(dLat/2)+qCos(qDegreesToRadians(a))*qCos(qDegreesToRadians(c))*qSin(dLon/2)*qSin(dLon/2);
    return radius*2*qAtan2(qSqrt(value),qSqrt(1-value));
}
void HomeView::locate()
{
    const QString query=search_->text().trimmed();
    if (query.contains(QStringLiteral("北京"))) { latitude_=39.9042; longitude_=116.4074; location_->setText(QStringLiteral("📍 北京市")); }
    else if (query.contains(QStringLiteral("沈阳"))) { latitude_=41.8057; longitude_=123.4315; location_->setText(QStringLiteral("📍 沈阳市")); }
    else { latitude_=22.543096; longitude_=114.057865; location_->setText(QStringLiteral("📍 深圳市")); }
    render(); emit locationChanged();
}
void HomeView::render()
{
    while (auto* item=cards_->takeAt(0)) { delete item->widget(); delete item; }
    QList<QPair<double,QJsonObject>> values;
    const QString filter=search_->text().trimmed();
    for (const auto& value:stations_) {
        auto station=value.toObject();
        if (!filter.isEmpty() && !filter.contains(QStringLiteral("深圳")) && !filter.contains(QStringLiteral("北京")) && !filter.contains(QStringLiteral("沈阳"))
            && !station.value(QStringLiteral("name")).toString().contains(filter,Qt::CaseInsensitive)
            && !station.value(QStringLiteral("address")).toString().contains(filter,Qt::CaseInsensitive)) continue;
        const double distance=distanceKm(latitude_,longitude_,station.value(QStringLiteral("latitude")).toDouble(),station.value(QStringLiteral("longitude")).toDouble());
        station.insert(QStringLiteral("distance_km"),distance); values.append({distance,station});
    }
    std::sort(values.begin(),values.end(),[](const auto& l,const auto& r){return l.first<r.first;});
    if(values.isEmpty()){auto* empty=new QLabel(QStringLiteral("没有找到匹配的充电站"),this);empty->setAlignment(Qt::AlignCenter);empty->setStyleSheet(QStringLiteral("color:#94a3b8;padding:40px;"));cards_->addWidget(empty);return;}
    for(const auto& entry:values){
        const auto station=entry.second; auto* card=new QPushButton(this); card->setObjectName(QStringLiteral("stationCard")); card->setMinimumHeight(112);
        card->setText(QStringLiteral("%1\n%2\n￥%3/度     %4 km     空闲 %5/%6")
            .arg(station.value(QStringLiteral("name")).toString(),station.value(QStringLiteral("address")).toString())
            .arg(station.value(QStringLiteral("price_per_kwh")).toDouble(),0,'f',2).arg(entry.first,0,'f',1)
            .arg(station.value(QStringLiteral("idle_pile_count")).toInt()).arg(station.value(QStringLiteral("pile_count")).toInt()));
        connect(card,&QPushButton::clicked,this,[this,station]{emit stationSelected(station);}); cards_->addWidget(card);
    }
}
}
