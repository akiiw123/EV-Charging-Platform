#pragma once
#include <QJsonArray>
#include <QWidget>
class QLabel; class QLineEdit; class QVBoxLayout;
namespace charging::user {
class HomeView final : public QWidget {
    Q_OBJECT
public:
    explicit HomeView(QWidget* parent = nullptr);
    void setStations(const QJsonArray& stations);
    double latitude() const { return latitude_; }
    double longitude() const { return longitude_; }
    QString locationName() const;
signals:
    void stationSelected(const QJsonObject& station);
    void locationChanged();
private:
    void render();
    void locate();
    static double distanceKm(double lat1, double lon1, double lat2, double lon2);
    QLabel* location_ = nullptr;
    QLineEdit* search_ = nullptr;
    QVBoxLayout* cards_ = nullptr;
    QJsonArray stations_;
    double latitude_ = 22.543096;
    double longitude_ = 114.057865;
};
}
