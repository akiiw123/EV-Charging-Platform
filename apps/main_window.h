#pragma once
#include "charging/core/api_client.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QTimer>
class QLabel; class QPushButton; class QStackedWidget;
namespace charging::user {
class LoginView; class HomeView; class StationDetailView; class ChargingView; class ProfileView; class MapNavigationView;
class MainWindow final : public QMainWindow {
    Q_OBJECT
public: explicit MainWindow(QWidget* parent=nullptr);
private:
    void buildUi(); void applyStyle(); void login(const QString& phone); void handleResponse(const charging::core::Message& message);
    void loadStations(); void loadPiles(qint64 stationId); void refreshAccount(); void showSection(int index);
    void updateUser(const QJsonObject& user); void updateOrder(const QJsonValue& value); void updateEstimate(); void logout();
    charging::core::ApiClient api_; QTimer timer_; QJsonObject user_; QJsonObject order_; QJsonObject selectedStation_; QJsonArray stations_; QJsonArray piles_;
    qint64 chargingSeconds_=0; double selectedPower_=0; double selectedPrice_=0;
    QStackedWidget* root_=nullptr; QStackedWidget* content_=nullptr; QLabel* topUser_=nullptr;
    LoginView* login_=nullptr; HomeView* home_=nullptr; StationDetailView* detail_=nullptr; ChargingView* charging_=nullptr; ProfileView* profile_=nullptr; MapNavigationView* map_=nullptr;
    QPushButton* homeTab_=nullptr; QPushButton* chargingTab_=nullptr; QPushButton* profileTab_=nullptr;
};
}
