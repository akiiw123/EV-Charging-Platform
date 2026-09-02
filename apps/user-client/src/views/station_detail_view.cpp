#include "station_detail_view.h"
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace charging::user {
namespace { QString pileState(const QString& s){if(s==QStringLiteral("idle"))return QStringLiteral("闲置");if(s==QStringLiteral("charging"))return QStringLiteral("使用中");if(s==QStringLiteral("fault"))return QStringLiteral("故障");return QStringLiteral("离线");} }
StationDetailView::StationDetailView(QWidget* parent):QWidget(parent)
{
    auto* root=new QVBoxLayout(this);root->setContentsMargins(26,18,26,18);root->setSpacing(12);
    auto* back=new QPushButton(QStringLiteral("← 返回充电站列表"),this);back->setObjectName(QStringLiteral("textButton"));
    summary_=new QLabel(this);summary_->setWordWrap(true);summary_->setObjectName(QStringLiteral("summaryCard"));summary_->setMinimumHeight(125);
    auto* nav=new QHBoxLayout;auto* drive=new QPushButton(QStringLiteral("🚗 驾车导航"),this);auto* walk=new QPushButton(QStringLiteral("🚶 步行导航"),this);
    drive->setObjectName(QStringLiteral("primaryButton"));walk->setObjectName(QStringLiteral("secondaryButton"));nav->addWidget(drive);nav->addWidget(walk);
    auto* heading=new QLabel(QStringLiteral("站内电桩"),this);heading->setStyleSheet(QStringLiteral("font-size:20px;font-weight:700;color:#17324d;"));
    auto* scroll=new QScrollArea(this);scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);auto* host=new QWidget; piles_=new QVBoxLayout(host);piles_->setAlignment(Qt::AlignTop);scroll->setWidget(host);
    root->addWidget(back,0,Qt::AlignLeft);root->addWidget(summary_);root->addLayout(nav);root->addWidget(heading);root->addWidget(scroll,1);
    connect(back,&QPushButton::clicked,this,&StationDetailView::backRequested);
    connect(drive,&QPushButton::clicked,this,[this]{emit navigationRequested(QStringLiteral("drive"),station_);});
    connect(walk,&QPushButton::clicked,this,[this]{emit navigationRequested(QStringLiteral("walk"),station_);});
}
void StationDetailView::setStation(const QJsonObject& station)
{
    station_=station;const int total=station.value(QStringLiteral("pile_count")).toInt(),idle=station.value(QStringLiteral("idle_pile_count")).toInt();
    const double online=total?100.0*(total-station.value(QStringLiteral("offline_count")).toInt())/total:0;
    summary_->setText(QStringLiteral("%1\n%2\n\n￥%3/度     总桩 %4     空闲 %5     在线率 %6%")
        .arg(station.value(QStringLiteral("name")).toString(),station.value(QStringLiteral("address")).toString())
        .arg(station.value(QStringLiteral("price_per_kwh")).toDouble(),0,'f',2).arg(total).arg(idle).arg(online,0,'f',0));
}
void StationDetailView::setPiles(const QJsonArray& values,bool canReserve)
{
    while(auto* item=piles_->takeAt(0)){delete item->widget();delete item;}
    for(const auto& value:values){const auto pile=value.toObject();const QString state=pile.value(QStringLiteral("status")).toString();
        auto* card=new QWidget(this);card->setObjectName(QStringLiteral("pileCard"));auto* row=new QHBoxLayout(card);
        const QString type=pile.value(QStringLiteral("type")).toString()==QStringLiteral("fast")?QStringLiteral("快充"):QStringLiteral("慢充");
        auto* info=new QLabel(QStringLiteral("%1\n%2 · %3kW").arg(pile.value(QStringLiteral("code")).toString(),type).arg(pile.value(QStringLiteral("power_kw")).toDouble(),0,'f',1),card);
        auto* status=new QLabel(pileState(state),card);status->setProperty("state",state);
        row->addWidget(info,1);row->addWidget(status);if(state==QStringLiteral("idle")){auto* reserve=new QPushButton(QStringLiteral("预约充电"),card);reserve->setEnabled(canReserve);connect(reserve,&QPushButton::clicked,this,[this,pile]{emit reservationRequested(pile.value(QStringLiteral("id")).toInteger(),pile.value(QStringLiteral("power_kw")).toDouble());});row->addWidget(reserve);}piles_->addWidget(card);
    }
}
}
