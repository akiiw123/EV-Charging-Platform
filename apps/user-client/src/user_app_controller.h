#pragma once

#include "charging/core/api_client.h"

#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace charging::user {

class UserAppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(QString noticeKind READ noticeKind NOTIFY noticeChanged)
    Q_PROPERTY(QString lastPhone READ lastPhone CONSTANT)
    Q_PROPERTY(QVariantMap user READ user NOTIFY userChanged)
    Q_PROPERTY(QVariantMap activeOrder READ activeOrder NOTIFY activeOrderChanged)
    Q_PROPERTY(QVariantMap selectedStation READ selectedStation NOTIFY selectedStationChanged)
    Q_PROPERTY(QVariantList stations READ stations NOTIFY stationsChanged)
    Q_PROPERTY(QVariantList piles READ piles NOTIFY pilesChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QString locationName READ locationName NOTIFY locationChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY locationChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY locationChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(QString chargingEstimate READ chargingEstimate NOTIFY chargingEstimateChanged)
    Q_PROPERTY(QUrl mapUrl READ mapUrl NOTIFY mapChanged)
    Q_PROPERTY(QString mapTitle READ mapTitle NOTIFY mapChanged)

public:
    explicit UserAppController(QObject* parent = nullptr);

    bool connected() const;
    bool loggedIn() const;
    bool busy() const;
    QString notice() const;
    QString noticeKind() const;
    QString lastPhone() const;
    QVariantMap user() const;
    QVariantMap activeOrder() const;
    QVariantMap selectedStation() const;
    QVariantList stations() const;
    QVariantList piles() const;
    QVariantList history() const;
    QString locationName() const;
    double latitude() const;
    double longitude() const;
    QString searchQuery() const;
    void setSearchQuery(const QString& value);
    QString chargingEstimate() const;
    QUrl mapUrl() const;
    QString mapTitle() const;

    Q_INVOKABLE void login(const QString& phone);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void refreshStations();
    Q_INVOKABLE void locate(const QString& address);
    Q_INVOKABLE void selectStation(const QVariantMap& station);
    Q_INVOKABLE void reserve(qint64 pileId, double powerKw);
    Q_INVOKABLE void orderAction(const QString& action);
    Q_INVOKABLE void refreshProfile();
    Q_INVOKABLE void updateNickname(const QString& nickname);
    // 头像:打开系统文件选择器,校验后裁成圆形 PNG 存到应用数据目录并上传路径
    Q_INVOKABLE void pickAvatar();
    Q_INVOKABLE void recharge(double amount);
    Q_INVOKABLE void openNavigation(const QString& mode);
    Q_INVOKABLE void clearNotice();

signals:
    void connectedChanged();
    void loggedInChanged();
    void busyChanged();
    void noticeChanged();
    void userChanged();
    void activeOrderChanged();
    void selectedStationChanged();
    void stationsChanged();
    void pilesChanged();
    void historyChanged();
    void locationChanged();
    void searchQueryChanged();
    void chargingEstimateChanged();
    void mapChanged();
    void loginSucceeded();
    void authenticationRejected();
    void reservationSucceeded();

private:
    void handleResponse(const charging::core::Message& message);
    void setBusy(bool value);
    void showNotice(const QString& text, const QString& kind = QStringLiteral("info"));
    void updateUser(const QVariantMap& value);
    void updateOrder(const QVariant& value);
    void rebuildStations();
    void loadPiles(qint64 stationId);
    void updateChargingEstimate();
    static double distanceKm(double lat1, double lon1, double lat2, double lon2);

    charging::core::ApiClient api_;
    QTimer chargingTimer_;
    QTimer noticeTimer_;
    bool connected_ = false;
    bool loggedIn_ = false;
    bool busy_ = false;
    QString notice_;
    QString noticeKind_ = QStringLiteral("info");
    QVariantMap user_;
    QVariantMap activeOrder_;
    QVariantMap selectedStation_;
    QVariantList rawStations_;
    QVariantList stations_;
    QVariantList piles_;
    QVariantList history_;
    QString locationName_ = QStringLiteral("深圳市");
    double latitude_ = 22.543096;
    double longitude_ = 114.057865;
    QString searchQuery_;
    qint64 chargingSeconds_ = 0;
    double selectedPowerKw_ = 0.0;
    double selectedPrice_ = 0.0;
    QString chargingEstimate_;
    QUrl mapUrl_;
    QString mapTitle_;
};

} // namespace charging::user
