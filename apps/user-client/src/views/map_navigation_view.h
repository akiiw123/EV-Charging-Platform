#pragma once
#include <QJsonObject>
#include <QWidget>
class QLabel; class QWebEngineView;
namespace charging::user {
class MapNavigationView final : public QWidget {
    Q_OBJECT
public:
    explicit MapNavigationView(QWidget* parent=nullptr);
    void navigate(const QString& mode,double fromLat,double fromLng,const QString& fromName,const QJsonObject& station);
signals:void backRequested();
private: QLabel* title_=nullptr; QWebEngineView* web_=nullptr;
};
}
