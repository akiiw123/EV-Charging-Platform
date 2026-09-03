#pragma once

#include "charging/core/api_client.h"
#include "json_list_model.h"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QTimer>

namespace charging::admin {

class AdminAppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(bool databaseReady READ databaseReady CONSTANT)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY noticeChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(QString noticeKind READ noticeKind NOTIFY noticeChanged)
    Q_PROPERTY(QString administrator READ administrator NOTIFY loggedInChanged)
    Q_PROPERTY(QString currentTime READ currentTime NOTIFY currentTimeChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool sidebarExpanded READ sidebarExpanded WRITE setSidebarExpanded NOTIFY settingsChanged)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled NOTIFY settingsChanged)
    Q_PROPERTY(double fontScale READ fontScale WRITE setFontScale NOTIFY settingsChanged)
    Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap dashboard READ dashboard NOTIFY dashboardChanged)
    Q_PROPERTY(QVariantList revenueTrend READ revenueTrend NOTIFY dashboardChanged)
    Q_PROPERTY(QVariantMap pileStatus READ pileStatus NOTIFY dashboardChanged)
    Q_PROPERTY(QVariantList stationEnergy READ stationEnergy NOTIFY dashboardChanged)
    Q_PROPERTY(QAbstractItemModel* stationsModel READ stationsModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel* pilesModel READ pilesModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel* ordersModel READ ordersModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel* usersModel READ usersModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel* predictionsModel READ predictionsModel CONSTANT)
    Q_PROPERTY(QString predictionSource READ predictionSource NOTIFY predictionChanged)
    Q_PROPERTY(QString predictionStatus READ predictionStatus NOTIFY predictionChanged)
    Q_PROPERTY(QString predictionUpdatedAt READ predictionUpdatedAt NOTIFY predictionChanged)
    Q_PROPERTY(bool mustChangePassword READ mustChangePassword NOTIFY mustChangePasswordChanged)
    // 真实预测聚合值(全部站点合计);演示模式为 "—"
    Q_PROPERTY(QString predictionLoad1 READ predictionLoad1 NOTIFY predictionChanged)
    Q_PROPERTY(QString predictionLoad6 READ predictionLoad6 NOTIFY predictionChanged)
    Q_PROPERTY(QString predictionLoad24 READ predictionLoad24 NOTIFY predictionChanged)
    Q_PROPERTY(QString predictionConfidence READ predictionConfidence NOTIFY predictionChanged)

public:
    explicit AdminAppController(bool databaseReady, QObject* parent = nullptr);
    bool connected() const { return connected_; }
    bool databaseReady() const { return databaseReady_; }
    bool loggedIn() const { return loggedIn_; }
    bool busy() const { return busyCount_ > 0; }
    QString errorMessage() const { return errorMessage_; }
    QString notice() const { return notice_; }
    QString noticeKind() const { return noticeKind_; }
    QString administrator() const { return administrator_; }
    QString currentTime() const { return currentTime_; }
    QString theme() const { return theme_; }
    bool sidebarExpanded() const { return sidebarExpanded_; }
    bool animationsEnabled() const { return animationsEnabled_; }
    double fontScale() const { return fontScale_; }
    int pageSize() const { return pageSize_; }
    QVariantMap dashboard() const { return dashboard_; }
    QVariantList revenueTrend() const { return revenueTrend_; }
    QVariantMap pileStatus() const { return pileStatus_; }
    QVariantList stationEnergy() const { return stationEnergy_; }
    QAbstractItemModel* stationsModel() { return &stations_; }
    QAbstractItemModel* pilesModel() { return &piles_; }
    QAbstractItemModel* ordersModel() { return &orders_; }
    QAbstractItemModel* usersModel() { return &users_; }
    QAbstractItemModel* predictionsModel() { return &predictions_; }
    QString predictionSource() const { return predictionSource_; }
    QString predictionStatus() const { return predictionStatus_; }
    QString predictionUpdatedAt() const { return predictionUpdatedAt_; }
    QString predictionLoad1() const { return predictionLoad1_; }
    QString predictionLoad6() const { return predictionLoad6_; }
    QString predictionLoad24() const { return predictionLoad24_; }
    QString predictionConfidence() const { return predictionConfidence_; }
    bool mustChangePassword() const { return mustChangePassword_; }

    Q_INVOKABLE void login(const QString& username, const QString& password, bool remember);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void refreshAll();
    // days 为趋势统计区间(7 或 30 日),默认 30
    Q_INVOKABLE void refreshDashboard(int days = 30);
    Q_INVOKABLE void refreshStations(const QString& query = {});
    Q_INVOKABLE void refreshPiles(const QString& query = {}, const QString& station = {}, const QString& type = {}, const QString& status = {});
    Q_INVOKABLE void refreshOrders(const QString& query = {}, const QString& status = {});
    Q_INVOKABLE void refreshUsers(const QString& phone = {}, const QString& status = {});
    Q_INVOKABLE void createStation(const QVariantMap& form);
    Q_INVOKABLE void updateStation(const QVariantMap& form);
    Q_INVOKABLE void deleteStation(qint64 id);
    Q_INVOKABLE void restartPile(qint64 id);
    Q_INVOKABLE void createPile(const QVariantMap& form);
    Q_INVOKABLE void updatePile(const QVariantMap& form);
    Q_INVOKABLE void setPileStatus(qint64 id, const QString& status);
    // 供"新增电桩"对话框选择所属电站
    Q_INVOKABLE QStringList stationNames() const;
    Q_INVOKABLE void setUserStatus(qint64 id, const QString& status);
    Q_INVOKABLE void refreshPredictions();
    // 强制改密流程:校验当前密码并设置新密码(服务端 PBKDF2 落库,清除首登标志)
    Q_INVOKABLE void changePassword(const QString& oldPassword, const QString& newPassword);
    Q_INVOKABLE void clearNotice();
    Q_INVOKABLE QString savedUsername() const;
    Q_INVOKABLE QVariantMap stationAt(int row) const { return stations_.get(row); }
    Q_INVOKABLE QVariantMap pileAt(int row) const { return piles_.get(row); }
    Q_INVOKABLE QVariantMap orderAt(int row) const { return orders_.get(row); }
    Q_INVOKABLE QVariantMap userAt(int row) const { return users_.get(row); }

    void setTheme(const QString& value);
    void setSidebarExpanded(bool value);
    void setAnimationsEnabled(bool value);
    void setFontScale(double value);
    void setPageSize(int value);

signals:
    void connectionChanged();
    void loggedInChanged();
    void busyChanged();
    void noticeChanged();
    void currentTimeChanged();
    void themeChanged();
    void settingsChanged();
    void dashboardChanged();
    void predictionChanged();
    void mustChangePasswordChanged();
    void passwordChangeResult(bool success);

private:
    void request(const QString& type, const QJsonObject& payload = {});
    void handleResponse(const charging::core::Message& message);
    void showNotice(const QString& text, const QString& kind = QStringLiteral("success"));
    void applyClientFilters();
    void usePredictionDemo(const QString& reason);
    void requestStationForecasts(const QJsonArray& stations);
    void applyForecastReply(int row, const QJsonObject& payload);
    void finishForecasts();

    charging::core::ApiClient api_;
    QNetworkAccessManager network_;
    QSettings settings_;
    QTimer clock_;
    bool databaseReady_ = true;
    bool connected_ = false;
    bool loggedIn_ = false;
    int busyCount_ = 0;
    QString errorMessage_, notice_, noticeKind_, administrator_, currentTime_;
    QString theme_;
    bool sidebarExpanded_ = true, animationsEnabled_ = true;
    double fontScale_ = 1.0;
    int pageSize_ = 20;
    QVariantMap dashboard_, pileStatus_;
    QVariantList revenueTrend_, stationEnergy_;
    JsonListModel stations_, piles_, orders_, users_, predictions_;
    QJsonArray rawStations_, rawPiles_, rawOrders_, rawUsers_;
    QString stationQuery_, pileQuery_, pileStation_, pileType_, pileState_, orderQuery_, orderState_, userQuery_, userState_;
    QString predictionSource_ = QStringLiteral("演示数据"), predictionStatus_ = QStringLiteral("未连接预测服务"), predictionUpdatedAt_;
    // /predict 请求的进行中状态与聚合结果
    QList<QVariantMap> pendingForecastRows_;
    int pendingForecastCount_ = 0;
    int forecastOkCount_ = 0;
    double forecastLoadSum_[3] = {0.0, 0.0, 0.0};   // 1/6/24h 合计 kWh
    double forecastConfidence_ = 0.0;               // 由 quantiles 推出,如 90
    QString predictionLoad1_ = QStringLiteral("—"), predictionLoad6_ = QStringLiteral("—"),
            predictionLoad24_ = QStringLiteral("—"), predictionConfidence_ = QStringLiteral("—");
    bool mustChangePassword_ = false;
};

} // namespace charging::admin

