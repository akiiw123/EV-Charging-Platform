/**
 * @file user_app_controller.h
 * @brief 用户端 QML 与 TCP 服务之间的 ViewModel 接口：向 QML 暴露属性、模型、命令和状态信号。
 *
 * 调用链说明：QML 调用 Q_INVOKABLE/槽函数，Controller 通过 ApiClient 异步访问服务端，
 * 收到响应后更新 Q_PROPERTY 或列表模型并发出信号，QML 绑定会自动刷新。
 */
#pragma once

#include "charging/core/api_client.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QHash>
#include <QJsonObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace charging::user {

/** 用户端页面的唯一业务入口。页面读取 Q_PROPERTY、调用 Q_INVOKABLE；该类负责校验、异步请求和状态同步。 */
class UserAppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool locating READ locating NOTIFY locatingChanged)
    Q_PROPERTY(QVariantMap filters READ filters NOTIFY stationsChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(QString noticeKind READ noticeKind NOTIFY noticeChanged)
    Q_PROPERTY(QString lastPhone READ lastPhone CONSTANT)
    Q_PROPERTY(QVariantMap user READ user NOTIFY userChanged)
    Q_PROPERTY(QVariantMap activeOrder READ activeOrder NOTIFY activeOrderChanged)
    Q_PROPERTY(QVariantMap selectedStation READ selectedStation NOTIFY selectedStationChanged)
    // 当前选中电站/活动订单所属电站的计价规则(station.pricing 响应):
    // {station_id, rule:{...}|null, periods:[...], current_price_per_kwh, fixed_price_per_kwh}
    Q_PROPERTY(QVariantMap pricing READ pricing NOTIFY pricingChanged)
    Q_PROPERTY(QVariantList stations READ stations NOTIFY stationsChanged)
    Q_PROPERTY(QVariantList piles READ piles NOTIFY pilesChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QVariantList rechargeHistory READ rechargeHistory NOTIFY rechargeHistoryChanged)
    Q_PROPERTY(QString locationName READ locationName NOTIFY locationChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY locationChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY locationChanged)
    // 是否配置了腾讯地图 Key:决定能否对任意地址做真实地理编码
    Q_PROPERTY(bool mapKeyConfigured READ mapKeyConfigured NOTIFY locationChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(QString chargingEstimate READ chargingEstimate NOTIFY chargingEstimateChanged)
    Q_PROPERTY(QUrl mapUrl READ mapUrl NOTIFY mapChanged)
    Q_PROPERTY(QString mapTitle READ mapTitle NOTIFY mapChanged)

public:
    explicit UserAppController(QObject* parent = nullptr);

    // 下列 getter 是 Q_PROPERTY 的数据出口；QML 通过属性绑定读取，状态改变时由对应 signal 通知刷新。
    QString theme() const { return theme_; }
    void setTheme(const QString& value);
    Q_INVOKABLE QString displayTime(const QString& value) const;
    bool connected() const;
    bool loggedIn() const;
    bool busy() const;
    bool locating() const { return locating_; }
    QVariantMap filters() const;
    QString notice() const;
    QString noticeKind() const;
    QString lastPhone() const;
    QVariantMap user() const;
    QVariantMap activeOrder() const;
    QVariantMap selectedStation() const;
    QVariantMap pricing() const { return pricing_; }
    QVariantList stations() const;
    QVariantList piles() const;
    QVariantList history() const;
    QVariantList rechargeHistory() const;
    QString locationName() const;
    bool mapKeyConfigured() const { return !qEnvironmentVariableIsEmpty("TENCENT_MAP_KEY"); }
    double latitude() const;
    double longitude() const;
    QString searchQuery() const;
    void setSearchQuery(const QString& value);
    QString chargingEstimate() const;
    QUrl mapUrl() const;
    QString mapTitle() const;

    // setFilters 保存首页筛选条件并重建可见站点；不修改服务端原始数据。
    Q_INVOKABLE void setFilters(double minDistance, double maxDistance, double minPrice, double maxPrice, const QString& type, bool idleOnly);
    // orderStatusText 只负责把协议英文状态翻译成中文显示文字。
    Q_INVOKABLE QString orderStatusText(const QString& status) const;
    // login/logout 建立或清理用户会话；登录请求成功后再加载用户、订单和站点数据。
    Q_INVOKABLE void login(const QString& phone);
    Q_INVOKABLE void logout();
    // refreshStations 从服务端重新拉取电站，再逐站补充电桩数据并应用本地距离/价格筛选。
    Q_INVOKABLE void refreshStations();
    // locate 优先识别预设城市；配置地图 Key 时也可异步地理编码任意地址。
    Q_INVOKABLE void locate(const QString& address);
    Q_INVOKABLE QVariantList presetCities() const { return presetCities_; }
    // selectStation 切换详情页上下文，并触发该站电桩和计价规则加载。
    Q_INVOKABLE void selectStation(const QVariantMap& station);
    // 拉取电站计价规则:选中电站与活动订单换站时自动调用
    Q_INVOKABLE void loadPricing(qint64 stationId);
    // reserve 发起预约；orderAction 根据当前订单状态发送开始、停止、结算或取消命令。
    Q_INVOKABLE void reserve(qint64 pileId, double powerKw);
    Q_INVOKABLE void orderAction(const QString& action);
    // 个人中心相关命令：刷新资料、修改昵称、选取头像和钱包充值。
    Q_INVOKABLE void refreshProfile();
    Q_INVOKABLE void updateNickname(const QString& nickname);
    // 头像:打开系统文件选择器,校验后裁成圆形 PNG 存到应用数据目录并上传路径
    Q_INVOKABLE void pickAvatar();
    Q_INVOKABLE void recharge(double amount);
    // openNavigation 根据 mode 生成腾讯地图驾车/公交/步行导航地址，交给 MapPage 展示。
    Q_INVOKABLE void openNavigation(const QString& mode);
    Q_INVOKABLE void clearNotice();

signals:
    void themeChanged();
    void connectedChanged();
    void loggedInChanged();
    void busyChanged();
    void locatingChanged();
    void noticeChanged();
    void userChanged();
    void activeOrderChanged();
    void selectedStationChanged();
    void pricingChanged();
    void stationsChanged();
    void pilesChanged();
    void historyChanged();
    void rechargeHistoryChanged();
    void locationChanged();
    void searchQueryChanged();
    void chargingEstimateChanged();
    void mapChanged();
    void loginSucceeded();
    void rechargeRequired();
    void rechargeSucceeded();
    void authenticationRejected();
    void reservationSucceeded();

private:
    // sendRequest 生成请求 ID 并登记 pending_；handleResponse 按 ID 找回原请求并更新对应状态。
    QString sendRequest(const QString& type, const QJsonObject& payload = {});
    void clearSession();
    void loadNextFilterPiles();
    QHash<QString, QString> pending_;
    QHash<qint64, QVariantList> stationPiles_;
    QList<qint64> filterQueue_;
    qint64 loadingStation_ = 0;
    QTimer requestTimer_;
    bool locating_ = false;
    quint64 session_ = 0;
    double minDistance_ = 0, maxDistance_ = -1, minPrice_ = 0, maxPrice_ = -1;
    QString pileType_;
    bool idleOnly_ = false;
    void handleResponse(const charging::core::Message& message);
    void setBusy(bool value);
    void showNotice(const QString& text, const QString& kind = QStringLiteral("info"));
    void updateUser(const QVariantMap& value);
    void updateOrder(const QVariant& value);
    void rebuildStations();
    void loadPiles(qint64 stationId);
    void updateChargingEstimate();
    // 充电实时估算金额:站点启用分时电价时按时段逐段取价,否则用固定单价
    double estimatedAmountFor(qint64 durationSeconds) const;
    void applyLocation(const QString& name, double latitude, double longitude);
    void geocodeAddress(const QString& address);
    QNetworkAccessManager network_;
    QVariantList presetCities_{
        QStringLiteral("北京"), QStringLiteral("上海"), QStringLiteral("广州"),
        QStringLiteral("深圳"), QStringLiteral("沈阳"), QStringLiteral("杭州")};
    static double distanceKm(double lat1, double lon1, double lat2, double lon2);

    charging::core::ApiClient api_;
    QTimer refreshTimer_;
    QString theme_ = QStringLiteral("default");
    QTimer chargingTimer_;
    QTimer noticeTimer_;
    bool connected_ = false;
    bool loggedIn_ = false;
    bool busy_ = false;
    // 本连接因账号在其他客户端登录而被服务端接管下线,
    // 用于在随后的断线回调中保留明确原因,不被通用断线提示覆盖
    bool sessionTakenOver_ = false;
    QString notice_;
    QString noticeKind_ = QStringLiteral("info");
    QVariantMap user_;
    QVariantMap activeOrder_;
    QVariantMap selectedStation_;
    // 选中/活动订单电站的计价规则缓存(station.pricing 响应)
    QVariantMap pricing_;
    qint64 pricingStationId_ = 0;
    QVariantList rawStations_;
    QVariantList stations_;
    QVariantList piles_;
    QVariantList history_;
    QVariantList rechargeHistory_;
    QString locationName_ = QStringLiteral("北京理工大学良乡校区");
    double latitude_ = 39.7296;
    double longitude_ = 116.1710;
    QString searchQuery_;
    qint64 chargingSeconds_ = 0;
    double selectedPowerKw_ = 0.0;
    double selectedPrice_ = 0.0;
    QString chargingEstimate_;
    QUrl mapUrl_;
    QString mapTitle_;
};

} // namespace charging::user
