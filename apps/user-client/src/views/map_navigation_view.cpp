#include "map_navigation_view.h"
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWebEngineView>
namespace charging::user {
MapNavigationView::MapNavigationView(QWidget* parent):QWidget(parent)
{
    auto* root=new QVBoxLayout(this);root->setContentsMargins(18,14,18,14);auto* row=new QHBoxLayout;auto* back=new QPushButton(QStringLiteral("← 返回电站详情"),this);title_=new QLabel(QStringLiteral("地图导航"),this);title_->setStyleSheet(QStringLiteral("font-size:20px;font-weight:700;"));row->addWidget(back);row->addWidget(title_,1);web_=new QWebEngineView(this);root->addLayout(row);root->addWidget(web_,1);connect(back,&QPushButton::clicked,this,&MapNavigationView::backRequested);
}
void MapNavigationView::navigate(const QString& mode,double lat,double lng,const QString& fromName,const QJsonObject& station)
{
    title_->setText((mode==QStringLiteral("walk")?QStringLiteral("步行导航 · "):QStringLiteral("驾车导航 · "))+station.value(QStringLiteral("name")).toString());
    QUrl url(QStringLiteral("https://apis.map.qq.com/uri/v1/routeplan"));QUrlQuery q;
    q.addQueryItem(QStringLiteral("type"),mode);q.addQueryItem(QStringLiteral("from"),fromName);q.addQueryItem(QStringLiteral("fromcoord"),QStringLiteral("%1,%2").arg(lat,0,'f',6).arg(lng,0,'f',6));
    q.addQueryItem(QStringLiteral("to"),station.value(QStringLiteral("name")).toString());q.addQueryItem(QStringLiteral("tocoord"),QStringLiteral("%1,%2").arg(station.value(QStringLiteral("latitude")).toDouble(),0,'f',6).arg(station.value(QStringLiteral("longitude")).toDouble(),0,'f',6));
    const QString key=qEnvironmentVariable("TENCENT_MAP_KEY");
    q.addQueryItem(QStringLiteral("referer"),QStringLiteral("charging-platform"));
    if(!key.isEmpty())q.addQueryItem(QStringLiteral("key"),key);
    url.setQuery(q);web_->load(url);
}
}
