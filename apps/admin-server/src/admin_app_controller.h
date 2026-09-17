/**
 * @file admin_app_controller.h
 * @brief 管理端 QML 与服务端/ML 服务之间的 ViewModel 接口：暴露表格模型、筛选状态和管理命令。
 *
 * 调用链说明：QML 调用 Q_INVOKABLE/槽函数，Controller 通过 ApiClient 异步访问服务端，
 * 收到响应后更新 Q_PROPERTY 或列表模型并发出信号，QML 绑定会自动刷新。
 */
#pragma once

#include "charging/core/api_client.h"
#include "json_list_model.h"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QHash>

namespace charging::admin {

/** 管理端页面的统一 ViewModel。它把 TCP/ML 响应整理成 QML 模型，并集中处理筛选、忙碌态和错误反馈。 */
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
    Q_PROPERTY(bool predictionLoading READ predictionLoading NOTIFY predictionChanged)
    Q_PROPERTY(QString predictionModelName READ predictionModelName NOTIFY predictionChanged)
    Q_PROPERTY(QString predictionMethod READ predictionMethod NOTIFY predictionChanged)
    Q_PROPERTY(QString predictionDataScope READ predictionDataScope NOTIFY predictionChanged)
    Q_PROPERTY(QString predictionCaveat READ predictionCaveat NOTIFY predictionChanged)
    Q_PROPERTY(bool mustChangePassword READ mustChangePassword NOTIFY mustChangePasswordChanged)
    Q_PROPERTY(bool loadFailed READ loadFailed NOTIFY loadFailedChanged)
    // 真实预测聚合值(当前展示站点合计);不可用时为 "—"
    Q_PROPERTY(QString predictionLoad1 READ predictionLoad1 NOTIFY predictionChanged)
    Q_PROPERTY(QString predictionLoad6 READ predictionLoad6 NOTIFY predictionChanged)
    Q_PROPERTY(QString predictionLoad24 READ predictionLoad24 NOTIFY predictionChanged)
    Q_PROPERTY(QString predictionConfidence READ predictionConfidence NOTIFY predictionChanged)
    // 站点计价规则编辑:{rule:{...}|null, periods:[...]}
    Q_PROPERTY(QVariantMap pricingDetail READ pricingDetail NOTIFY pricingChanged)

public:
    explicit AdminAppController(bool databaseReady, QObject* parent = nullptr);
    // getter 为 QML 属性绑定提供当前快照；列表通过 QAbstractItemModel 暴露，避免在 QML 硬编码表格行。
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
    bool predictionLoading() const { return predictionLoading_; }
    QString predictionModelName() const { return predictionModelName_; }
    QString predictionMethod() const { return predictionMethod_; }
    QString predictionDataScope() const { return predictionDataScope_; }
    QString predictionCaveat() const { return predictionCaveat_; }
    QString predictionLoad1() const { return predictionLoad1_; }
    QString predictionLoad6() const { return predictionLoad6_; }
    QString predictionLoad24() const { return predictionLoad24_; }
    QString predictionConfidence() const { return predictionConfidence_; }
    QVariantMap pricingDetail() const { return pricingDetail_; }
    bool mustChangePassword() const { return mustChangePassword_; }
    bool loadFailed() const { return loadFailed_; }

    // displayTime 将服务端 UTC 时间转换成界面统一使用的北京时间文本。
    Q_INVOKABLE QString displayTime(const QString& value) const;
    // login/logout 管理管理员会话；refreshAll 在登录成功后并行拉取各业务模块。
    Q_INVOKABLE void login(const QString& username, const QString& password, bool remember);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void refreshAll();
    // days 为趋势统计区间(7 或 30 日),默认 30
    Q_INVOKABLE void refreshDashboard(int days = 30);
    // refreshStations/Piles/Orders/Users 将搜索和筛选条件带入请求或本地过滤，再更新对应模型。
    Q_INVOKABLE void refreshStations(const QString& query = {});
    // stationIds 为电站 id 列表(QVariantList);空列表表示全部电桩
    Q_INVOKABLE void refreshPiles(const QString& query = {}, const QVariantList& stationIds = {}, const QString& type = {}, const QString& status = {});
    Q_INVOKABLE void refreshOrders(const QString& query = {}, const QString& status = {});
    Q_INVOKABLE void refreshUsers(const QString& phone = {}, const QString& status = {});
    // 三级区域筛选(电站管理页):任一级为空串表示"该级不限制";
    // 特殊值 kUnclassified("未分区")匹配区域字段为空的电站
    Q_INVOKABLE void setStationRegionFilter(const QString& province, const QString& city, const QString& district);
    // 级联选项:从当前电站数据的去重值生成,保证选了上级后下级选项一定非空
    Q_INVOKABLE QStringList stationProvinces() const;
    Q_INVOKABLE QStringList stationCities(const QString& province) const;
    Q_INVOKABLE QStringList stationDistricts(const QString& province, const QString& city) const;
    // 电桩管理页"所属电站"多选下拉数据源:全量电站(id/名称/电桩数)
    Q_INVOKABLE QVariantList allStationSummaries() const;
    // 常用电站推荐:本机记录的最近管理电站(最多 5 个,不区分管理员账号)
    Q_INVOKABLE QVariantList recentStations() const;
    Q_INVOKABLE void noteStationManaged(qint64 stationId);
    // create/update/deleteStation 与 create/update/restartPile 只发送管理命令；业务约束由服务端最终校验。
    Q_INVOKABLE void createStation(const QVariantMap& form);
    Q_INVOKABLE void updateStation(const QVariantMap& form);
    Q_INVOKABLE void deleteStation(qint64 id);
    Q_INVOKABLE void restartPile(qint64 id);
    Q_INVOKABLE void createPile(const QVariantMap& form);
    Q_INVOKABLE void updatePile(const QVariantMap& form);
    Q_INVOKABLE void setPileStatus(qint64 id, const QString& status);
    // 供"新增电桩"对话框选择所属电站
    Q_INVOKABLE QStringList stationNames() const;
    // setUserStatus 执行冻结或解冻；冻结后的在线会话由服务端负责失效处理。
    Q_INVOKABLE void setUserStatus(qint64 id, const QString& status);
    // 站点计价规则(分时电价段 + 占位费):编辑对话框打开时拉取,保存后立即对计费生效
    Q_INVOKABLE void loadPricing(qint64 stationId);
    Q_INVOKABLE void savePricing(const QVariantMap& form);
    // refreshPredictions 先读取 ML 站点目录，再异步请求各站 1/6/24 小时预测并聚合到模型。
    Q_INVOKABLE void refreshPredictions();
    // 强制改密流程:校验当前密码并设置新密码(服务端 PBKDF2 落库,清除首登标志)
    Q_INVOKABLE void changePassword(const QString& oldPassword, const QString& newPassword);
    Q_INVOKABLE void clearNotice();
    // 供 QML 直接使用全局提示条(如"去管理电桩"跳转后的上下文反馈)
    Q_INVOKABLE void notify(const QString& text, const QString& kind = QStringLiteral("success"));
    Q_INVOKABLE QString savedUsername() const;
    Q_INVOKABLE QVariantMap stationAt(int row) const { return stations_.get(row); }
    Q_INVOKABLE QVariantMap pileAt(int row) const { return piles_.get(row); }
    Q_INVOKABLE QVariantMap orderAt(int row) const { return orders_.get(row); }
    Q_INVOKABLE QVariantMap userAt(int row) const { return users_.get(row); }
    // 电站详情抽屉:按电站 id 过滤电桩列表(名称可能重名,id 才是稳定关联)
    Q_INVOKABLE QVariantList pilesOfStationId(qint64 stationId) const;

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
    void pricingChanged();
    void mustChangePasswordChanged();
    void loadFailedChanged();
    void passwordChangeResult(bool success);
    // 电站原始数据更新(管理端电站列表/编辑后),电桩页下拉等据此刷新
    void stationDataChanged();

private:
    // request 统一生成 TCP 请求并维护 busy/pending；handleResponse 按消息类型更新模型与提示。
    void request(const QString& type, const QJsonObject& payload = {});
    void handleResponse(const charging::core::Message& message);
    void showNotice(const QString& text, const QString& kind = QStringLiteral("success"));
    void applyClientFilters();
    void usePredictionDemo(const QString& reason);
    void requestStationForecasts(const QJsonObject& catalog, int generation);
    bool applyForecastReply(int row, const QJsonObject& payload);
    void finishForecasts();

    charging::core::ApiClient api_;
    QNetworkAccessManager network_;
    QSettings settings_;
    QTimer clock_, refreshTimer_, noticeTimer_, requestTimer_;
    QHash<QString, QString> pending_;
    bool connectionNotice_ = false;
    // 电站管理页三级区域筛选状态(空串=该级不限制)
    QString stationProvince_, stationCity_, stationDistrict_;
    // 电桩管理页"所属电站"多选(空集合=全部电桩)
    QSet<qint64> pileStationIds_;
    // 最近管理电站 id(本机 QSettings 持久化,新记录在前,最多 5 个)
    QStringList recentStationIds_;
    int dashboardDays_ = 30;
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
    // 计价规则编辑数据({rule:{...}|null, periods:[...]})
    QVariantMap pricingDetail_;
    JsonListModel stations_, piles_, orders_, users_, predictions_;
    QJsonArray rawStations_, rawPiles_, rawOrders_, rawUsers_;
    QString stationQuery_, pileQuery_, pileStation_, pileType_, pileState_, orderQuery_, orderState_, userQuery_, userState_;
    QString predictionSource_ = QStringLiteral("未连接"), predictionStatus_ = QStringLiteral("未连接预测服务"), predictionUpdatedAt_;
    // /predict 请求的进行中状态与聚合结果
    QList<QVariantMap> pendingForecastRows_;
    int pendingForecastCount_ = 0;
    int forecastOkCount_ = 0;
    double forecastLoadSum_[3] = {0.0, 0.0, 0.0};   // 1/6/24h 合计 kWh
    double forecastConfidence_ = 0.0;               // 由 quantiles 推出,如 90
    bool predictionLoading_ = false;
    int forecastRequestGeneration_ = 0;
    int forecastStationCount_ = 0;
    bool forecastCapacityVerified_ = true, forecastShiftedData_ = false;
    QString forecastMode_, forecastReplayAt_, forecastDateRange_, forecastModel_, forecastAlgorithm_,
            forecastLoadBasis_;
    QStringList forecastWarnings_;
    QString predictionLoad1_ = QStringLiteral("—"), predictionLoad6_ = QStringLiteral("—"),
            predictionLoad24_ = QStringLiteral("—"), predictionConfidence_ = QStringLiteral("—");
    QString predictionModelName_ = QStringLiteral("—"), predictionMethod_ = QStringLiteral("—"),
            predictionDataScope_ = QStringLiteral("—"), predictionCaveat_ = QStringLiteral("—");
    bool mustChangePassword_ = false;
    bool loadFailed_ = false;
};

} // namespace charging::admin
