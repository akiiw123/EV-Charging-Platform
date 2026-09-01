#pragma once

#include "charging/core/api_client.h"
#include <QJsonObject>
#include <QMainWindow>
#include <QTimer>

class QLabel; class QLineEdit; class QListWidget; class QPushButton;

namespace charging::user {

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void login();
    void handleResponse(const charging::core::Message& message);
    void loadStations();
    void loadPiles(qint64 stationId);
    void refreshAccount();
    void updateUser(const QJsonObject& user);
    void updateOrder(const QJsonValue& orderValue);
    void updateControls();
    void updateClock();

    charging::core::ApiClient api_;
    QTimer timer_;
    QJsonObject user_;
    QJsonObject order_;
    qint64 selectedStationId_ = 0;
    qint64 selectedPileId_ = 0;
    double selectedPower_ = 0;
    double selectedPrice_ = 0;
    qint64 chargingSeconds_ = 0;
    QLineEdit* phone_ = nullptr;
    QPushButton* loginButton_ = nullptr;
    QLabel* userLabel_ = nullptr;
    QListWidget* stations_ = nullptr;
    QListWidget* piles_ = nullptr;
    QLabel* orderLabel_ = nullptr;
    QLabel* chargingLabel_ = nullptr;
    QPushButton* reserve_ = nullptr;
    QPushButton* start_ = nullptr;
    QPushButton* stop_ = nullptr;
    QPushButton* settle_ = nullptr;
    QPushButton* cancel_ = nullptr;
    QLineEdit* nickname_ = nullptr;
    QLineEdit* avatar_ = nullptr;
    QLineEdit* recharge_ = nullptr;
    QLabel* accountLabel_ = nullptr;
    QListWidget* history_ = nullptr;
};

} // namespace charging::user
