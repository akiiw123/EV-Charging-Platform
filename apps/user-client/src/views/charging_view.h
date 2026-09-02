#pragma once
#include <QJsonObject>
#include <QWidget>
class QLabel; class QPushButton;
namespace charging::user {
class ChargingView final : public QWidget {
    Q_OBJECT
public:
    explicit ChargingView(QWidget* parent=nullptr);
    void setOrder(const QJsonValue& order);
    void setEstimate(qint64 seconds,double power,double price);
signals:
    void actionRequested(const QString& action);
private:
    QLabel* state_=nullptr; QLabel* estimate_=nullptr;
    QPushButton* start_=nullptr; QPushButton* stop_=nullptr; QPushButton* settle_=nullptr; QPushButton* cancel_=nullptr;
};
}
