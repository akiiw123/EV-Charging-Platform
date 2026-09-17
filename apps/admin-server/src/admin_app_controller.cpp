/**
 * @file admin_app_controller.cpp
 * @brief 管理端业务编排：发送 TCP 管理请求、更新列表模型、持久化界面设置并异步请求 ML 预测。
 *
 * 调用链说明：QML 调用 Q_INVOKABLE/槽函数，Controller 通过 ApiClient 异步访问服务端，
 * 收到响应后更新 Q_PROPERTY 或列表模型并发出信号，QML 绑定会自动刷新。
 */
#include "charging/core/display_time.h"
#include "admin_app_controller.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <cmath>

namespace charging::admin {
namespace {
bool containsCI(const QJsonObject& o, const QStringList& keys, const QString& needle) {
    if (needle.trimmed().isEmpty()) return true;
    for (const auto& key : keys) if (o.value(key).toVariant().toString().contains(needle, Qt::CaseInsensitive)) return true;
    return false;
}
// 三级区域筛选的"未分区"哨兵值:匹配区域字段为空的电站(旧数据/未填写)
const QString kUnclassified = QStringLiteral("未分区");
// selected 为空串表示该级不限制;为 kUnclassified 时匹配空字段
bool regionLevelMatches(const QString& value, const QString& selected) {
    if (selected.isEmpty()) return true;
    if (selected == kUnclassified) return value.isEmpty();
    return value == selected;
}
// 区域选项 = 数据中出现的去重值排序;不引入行政区划字典,保证没有死选项
QStringList sortedUniqueRegions(QSet<QString> values) {
    QStringList out;
    for (const auto& value : values) if (!value.isEmpty()) out << value;
    out.sort(Qt::CaseInsensitive);
    return out;
}
QString predictionAlgorithmLabel(const QString& value) {
    if (value == QStringLiteral("seasonal")) return QStringLiteral("季节均值");
    if (value == QStringLiteral("ml")) return QStringLiteral("机器学习模型");
    if (value == QStringLiteral("zero")) return QStringLiteral("零值基准");
    return value.isEmpty() ? QStringLiteral("未提供") : value;
}
}

AdminAppController::AdminAppController(bool databaseReady, QObject* parent)
    : QObject(parent), settings_(QStringLiteral("charging-platform"), QStringLiteral("admin-console")), databaseReady_(databaseReady)
{
    theme_ = settings_.value(QStringLiteral("appearance/theme"), QStringLiteral("default")).toString();
    const QString forcedTheme = qEnvironmentVariable("CHARGING_ADMIN_THEME");
    if (!forcedTheme.isEmpty()) theme_ = forcedTheme;
    sidebarExpanded_ = settings_.value(QStringLiteral("appearance/sidebarExpanded"), true).toBool();
    animationsEnabled_ = settings_.value(QStringLiteral("appearance/animations"), true).toBool();
    fontScale_ = settings_.value(QStringLiteral("appearance/fontScale"), 1.0).toDouble();
    pageSize_ = settings_.value(QStringLiteral("table/pageSize"), 20).toInt();
    recentStationIds_ = settings_.value(QStringLiteral("recent/stationIds")).toStringList();
    noticeTimer_.setSingleShot(true); noticeTimer_.setInterval(5000);
    connect(&noticeTimer_, &QTimer::timeout, this, &AdminAppController::clearNotice);
    requestTimer_.setSingleShot(true); requestTimer_.setInterval(15000);
    connect(&requestTimer_, &QTimer::timeout, this, [this] {
        pending_.clear(); busyCount_ = 0; emit busyChanged();
        logout(); showNotice(QStringLiteral("请求超时，请重新登录核对操作结果"), QStringLiteral("error"));
    });
    refreshTimer_.setInterval(5000);
    connect(&refreshTimer_, &QTimer::timeout, this, [this] { if (loggedIn_ && connected_ && !mustChangePassword_ && pending_.isEmpty()) refreshAll(); });
    refreshTimer_.start();
    clock_.setInterval(1000);
    connect(&clock_, &QTimer::timeout, this, [this] { currentTime_ = QDateTime::currentDateTimeUtc().toOffsetFromUtc(8 * 3600).toString(QStringLiteral("yyyy-MM-dd  HH:mm:ss")); emit currentTimeChanged(); });
    currentTime_ = QDateTime::currentDateTimeUtc().toOffsetFromUtc(8 * 3600).toString(QStringLiteral("yyyy-MM-dd  HH:mm:ss"));
    clock_.start();
    connect(&api_, &charging::core::ApiClient::connected, this, [this] { connected_ = true; if (connectionNotice_) clearNotice(); connectionNotice_ = false; emit connectionChanged(); });
    connect(&api_, &charging::core::ApiClient::disconnected, this, [this] { connected_ = false; loggedIn_ = false; pending_.clear(); requestTimer_.stop(); busyCount_ = 0; connectionNotice_ = true; showNotice(QStringLiteral("连接已断开，正在重连，请稍后重新登录"), QStringLiteral("warning")); emit connectionChanged(); emit loggedInChanged(); emit busyChanged(); });
    connect(&api_, &charging::core::ApiClient::clientError, this, [this](const QString& text) {
        if (!connected_) return; // Startup/reconnect progress is shown by the connection indicator.
        showNotice(text, QStringLiteral("error"));
    });
    // 过期响应不更新界面,仅归还 busy 计数
    connect(&api_, &charging::core::ApiClient::staleResponseReceived, this, [this](const charging::core::Message& m) { pending_.remove(m.id); busyCount_ = pending_.size(); if (pending_.isEmpty()) requestTimer_.stop(); emit busyChanged(); });
    connect(&api_, &charging::core::ApiClient::responseReceived, this, &AdminAppController::handleResponse);
    bool ok=false; const int port=qEnvironmentVariableIntValue("CHARGING_SERVER_PORT",&ok);
    api_.connectToServer(qEnvironmentVariable("CHARGING_SERVER_HOST", QStringLiteral("127.0.0.1")), ok && port>0 ? quint16(port) : quint16(45454));
    const QString smokePassword = qEnvironmentVariable("CHARGING_ADMIN_SMOKE_PASSWORD");
    if (!smokePassword.isEmpty()) {
        QTimer::singleShot(600, this, [this, smokePassword] { login(QStringLiteral("admin"), smokePassword, false); });
    }
}

QString AdminAppController::displayTime(const QString& value) const { return charging::core::beijingTime(value); }
void AdminAppController::setStationRegionFilter(const QString& province, const QString& city, const QString& district)
{
    if (stationProvince_ == province && stationCity_ == city && stationDistrict_ == district) return;
    stationProvince_ = province; stationCity_ = city; stationDistrict_ = district;
    applyClientFilters();
}

QStringList AdminAppController::stationProvinces() const
{
    QSet<QString> values;
    bool hasUnclassified = false;
    for (const auto& v : rawStations_) {
        const QString value = v.toObject().value(QStringLiteral("province")).toString();
        if (value.isEmpty()) hasUnclassified = true; else values.insert(value);
    }
    QStringList out = sortedUniqueRegions(values);
    if (hasUnclassified) out << kUnclassified;
    return out;
}

QStringList AdminAppController::stationCities(const QString& province) const
{
    QSet<QString> values;
    bool hasUnclassified = false;
    for (const auto& v : rawStations_) {
        const auto o = v.toObject();
        if (!regionLevelMatches(o.value(QStringLiteral("province")).toString(), province)) continue;
        const QString value = o.value(QStringLiteral("city")).toString();
        if (value.isEmpty()) hasUnclassified = true; else values.insert(value);
    }
    QStringList out = sortedUniqueRegions(values);
    if (hasUnclassified) out << kUnclassified;
    return out;
}

QStringList AdminAppController::stationDistricts(const QString& province, const QString& city) const
{
    QSet<QString> values;
    bool hasUnclassified = false;
    for (const auto& v : rawStations_) {
        const auto o = v.toObject();
        if (!regionLevelMatches(o.value(QStringLiteral("province")).toString(), province)) continue;
        if (!regionLevelMatches(o.value(QStringLiteral("city")).toString(), city)) continue;
        const QString value = o.value(QStringLiteral("district")).toString();
        if (value.isEmpty()) hasUnclassified = true; else values.insert(value);
    }
    QStringList out = sortedUniqueRegions(values);
    if (hasUnclassified) out << kUnclassified;
    return out;
}

QVariantList AdminAppController::allStationSummaries() const
{
    QVariantList out;
    for (const auto& v : rawStations_) {
        const auto o = v.toObject();
        out.append(QVariantMap{
            {QStringLiteral("id"), o.value(QStringLiteral("id")).toVariant()},
            {QStringLiteral("name"), o.value(QStringLiteral("name")).toString()},
            {QStringLiteral("pile_count"), o.value(QStringLiteral("pile_count")).toInt()}});
    }
    return out;
}

QVariantList AdminAppController::recentStations() const
{
    QVariantList out;
    for (const auto& idText : recentStationIds_) {
        const qint64 id = idText.toLongLong();
        for (const auto& v : rawStations_) {
            const auto o = v.toObject();
            if (o.value(QStringLiteral("id")).toVariant().toLongLong() != id) continue;
            out.append(QVariantMap{
                {QStringLiteral("id"), o.value(QStringLiteral("id")).toVariant()},
                {QStringLiteral("name"), o.value(QStringLiteral("name")).toString()},
                {QStringLiteral("pile_count"), o.value(QStringLiteral("pile_count")).toInt()}});
            break;
        }
    }
    return out;
}

void AdminAppController::noteStationManaged(qint64 stationId)
{
    if (stationId <= 0) return;
    const QString idText = QString::number(stationId);
    recentStationIds_.removeAll(idText);
    recentStationIds_.prepend(idText);
    while (recentStationIds_.size() > 5) recentStationIds_.removeLast();
    settings_.setValue(QStringLiteral("recent/stationIds"), recentStationIds_);
}

void AdminAppController::notify(const QString& text, const QString& kind) { showNotice(text, kind); }

void AdminAppController::loadPricing(qint64 stationId)
{
    if (stationId <= 0) return;
    request(QStringLiteral("admin.pricing.get"), {{QStringLiteral("station_id"), stationId}});
}

void AdminAppController::savePricing(const QVariantMap& form)
{
    request(QStringLiteral("admin.pricing.set"), QJsonObject::fromVariantMap(form));
}

void AdminAppController::request(const QString& type, const QJsonObject& payload) {
    if (!api_.isConnected()) { connectionNotice_ = true; showNotice(QStringLiteral("正在连接服务，请稍后重试"), QStringLiteral("info")); return; }
    if (!loggedIn_ && type != QStringLiteral("admin.login")) return;
    if (pending_.values().contains(type)) return;
    const QString id = api_.send(type, payload);
    if (id.isEmpty()) return;
    pending_.insert(id, type); busyCount_ = pending_.size(); emit busyChanged();
    if (!requestTimer_.isActive()) requestTimer_.start();
}

void AdminAppController::login(const QString& username,const QString& password,bool remember) { errorMessage_.clear(); if(username.trimmed().isEmpty()||password.isEmpty()){errorMessage_=QStringLiteral("请输入管理员账号和密码");emit noticeChanged();return;} settings_.setValue(QStringLiteral("login/username"),remember?username:QString()); request(QStringLiteral("admin.login"),{{"username",username},{"password",password}}); }
void AdminAppController::logout(){ pending_.clear(); requestTimer_.stop(); busyCount_=0; emit busyChanged(); loggedIn_=false; administrator_.clear(); if(mustChangePassword_){mustChangePassword_=false;emit mustChangePasswordChanged();} emit loggedInChanged(); }
void AdminAppController::changePassword(const QString& oldPassword,const QString& newPassword)
{
    // 前端先行做强度/一致性检查,服务端仍会二次校验(PBKDF2 验证旧密码)
    if (newPassword.size() < 8) { showNotice(QStringLiteral("新密码至少需要 8 位"), QStringLiteral("error")); emit passwordChangeResult(false); return; }
    request(QStringLiteral("admin.password.change"),{{QStringLiteral("old_password"),oldPassword},{QStringLiteral("new_password"),newPassword}});
}
void AdminAppController::refreshAll(){ if (!loggedIn_) return; refreshDashboard(dashboardDays_); request(QStringLiteral("admin.station.list")); request(QStringLiteral("admin.pile.list")); request(QStringLiteral("admin.order.list")); request(QStringLiteral("admin.user.list"),{{"phone",userQuery_}}); }
void AdminAppController::refreshDashboard(int days)
{
    dashboardDays_ = days == 7 ? 7 : 30;
    // 请求指定区间的运营总览,服务端按 days(7/30)返回营收趋势
    request(QStringLiteral("admin.dashboard"), {{QStringLiteral("days"), days}});
}
void AdminAppController::refreshStations(const QString& q){ stationQuery_=q; applyClientFilters(); request(QStringLiteral("admin.station.list")); }
void AdminAppController::refreshPiles(const QString& q, const QVariantList& stationIds, const QString& type, const QString& status)
{
    pileQuery_ = q;
    pileStationIds_.clear();
    for (const auto& value : stationIds) pileStationIds_.insert(value.toLongLong());
    // 单站筛选视为一次明确的"管理该站"动作,记入最近管理;多选时不推断意图
    if (pileStationIds_.size() == 1) noteStationManaged(*pileStationIds_.constBegin());
    pileType_ = type; pileState_ = status;
    applyClientFilters();
    request(QStringLiteral("admin.pile.list"));
}
void AdminAppController::refreshOrders(const QString& q,const QString& status){orderQuery_=q;orderState_=status;applyClientFilters();request(QStringLiteral("admin.order.list"));}
void AdminAppController::refreshUsers(const QString& q,const QString& status){userQuery_=q;userState_=status;request(QStringLiteral("admin.user.list"),{{"phone",q}});}
void AdminAppController::createStation(const QVariantMap& f){request(QStringLiteral("admin.station.create"),QJsonObject::fromVariantMap(f));}
void AdminAppController::updateStation(const QVariantMap& f){request(QStringLiteral("admin.station.update"),QJsonObject::fromVariantMap(f));}
void AdminAppController::deleteStation(qint64 id){request(QStringLiteral("admin.station.delete"),{{"station_id",id}});}
void AdminAppController::restartPile(qint64 id){request(QStringLiteral("admin.pile.restart"),{{"pile_id",id}});}
void AdminAppController::createPile(const QVariantMap& f){request(QStringLiteral("admin.pile.create"),QJsonObject::fromVariantMap(f));}
void AdminAppController::updatePile(const QVariantMap& f){request(QStringLiteral("admin.pile.update"),QJsonObject::fromVariantMap(f));}
void AdminAppController::setPileStatus(qint64 id,const QString& status){request(QStringLiteral("admin.pile.status"),{{"pile_id",id},{"status",status}});}
QVariantList AdminAppController::pilesOfStationId(qint64 stationId) const
{
    QVariantList out;
    for (const auto& v : rawPiles_) {
        const auto o = v.toObject();
        if (o.value(QStringLiteral("station_id")).toVariant().toLongLong() == stationId)
            out.append(o.toVariantMap());
    }
    return out;
}

QStringList AdminAppController::stationNames() const
{
    QStringList names;
    for (const auto& v : rawStations_) names << v.toObject().value(QStringLiteral("name")).toString();
    return names;
}
void AdminAppController::setUserStatus(qint64 id,const QString& status){request(QStringLiteral("admin.user.status"),{{"user_id",id},{"status",status}});}
void AdminAppController::clearNotice(){noticeTimer_.stop(); errorMessage_.clear(); notice_.clear();noticeKind_.clear();emit noticeChanged();}
QString AdminAppController::savedUsername() const{return settings_.value(QStringLiteral("login/username")).toString();}
void AdminAppController::setTheme(const QString& v){if(theme_==v)return;theme_=v;settings_.setValue("appearance/theme",v);emit themeChanged();}
void AdminAppController::setSidebarExpanded(bool v){if(sidebarExpanded_==v)return;sidebarExpanded_=v;settings_.setValue("appearance/sidebarExpanded",v);emit settingsChanged();}
void AdminAppController::setAnimationsEnabled(bool v){if(animationsEnabled_==v)return;animationsEnabled_=v;settings_.setValue("appearance/animations",v);emit settingsChanged();}
void AdminAppController::setFontScale(double v){v=qBound(.85,v,1.3);if(qFuzzyCompare(fontScale_,v))return;fontScale_=v;settings_.setValue("appearance/fontScale",v);emit settingsChanged();}
void AdminAppController::setPageSize(int v){v=qBound(10,v,100);if(pageSize_==v)return;pageSize_=v;settings_.setValue("table/pageSize",v);emit settingsChanged();}
void AdminAppController::showNotice(const QString&t,const QString&k){notice_=t;noticeKind_=k;if(k=="error")errorMessage_=t;else errorMessage_.clear();noticeTimer_.start();emit noticeChanged();}

void AdminAppController::handleResponse(const charging::core::Message& m){
    if (!pending_.contains(m.id)) return;
    pending_.remove(m.id); busyCount_ = pending_.size(); if (pending_.isEmpty()) requestTimer_.stop(); emit busyChanged(); if(m.type.endsWith(".error")){showNotice(m.payload.value("message").toString(),"error");if(m.type==QStringLiteral("admin.password.change.error"))emit passwordChangeResult(false);return;}
    if(m.type=="admin.login.ok"){clearNotice();loggedIn_=true;administrator_=m.payload.value("administrator").toObject().value("username").toString();const bool mustChange=m.payload.value("administrator").toObject().value("must_change_password").toBool();if(mustChange!=mustChangePassword_){mustChangePassword_=mustChange;emit mustChangePasswordChanged();}errorMessage_.clear();emit loggedInChanged();emit noticeChanged();refreshAll();return;}
    if(m.type=="admin.password.change.ok"){mustChangePassword_=false;emit mustChangePasswordChanged();showNotice(QStringLiteral("密码已更新，请使用新密码重新登录"));emit passwordChangeResult(true);logout();return;}
    if(m.type=="admin.dashboard.ok"){dashboard_=m.payload.value("metrics").toObject().toVariantMap();pileStatus_=m.payload.value("pile_status").toObject().toVariantMap();revenueTrend_=m.payload.value("revenue_trend").toArray().toVariantList();stationEnergy_=m.payload.value("station_energy").toArray().toVariantList();emit dashboardChanged();return;}
    if(m.type=="admin.station.list.ok")rawStations_=m.payload.value("stations").toArray();
    else if(m.type=="admin.pile.list.ok")rawPiles_=m.payload.value("piles").toArray();
    else if(m.type=="admin.order.list.ok")rawOrders_=m.payload.value("orders").toArray();
    else if(m.type=="admin.user.list.ok")rawUsers_=m.payload.value("users").toArray();
    else if(m.type=="admin.station.create.ok"||m.type=="admin.station.update.ok"||m.type=="admin.station.delete.ok"){showNotice(QStringLiteral("电站信息已更新"));rawStations_={};request("admin.station.list");request("admin.pile.list");refreshDashboard();return;}
    else if(m.type=="admin.pile.restart.ok"){showNotice(QStringLiteral("重启指令执行成功"));rawPiles_={};request("admin.pile.list");refreshDashboard();return;}
    else if(m.type=="admin.pile.create.ok"){showNotice(QStringLiteral("电桩已新增"));rawPiles_={};request("admin.pile.list");refreshDashboard();return;}
    else if(m.type=="admin.pile.update.ok"){showNotice(QStringLiteral("电桩信息已更新"));rawPiles_={};request("admin.pile.list");return;}
    else if(m.type=="admin.pile.status.ok"){showNotice(QStringLiteral("电桩状态已更新"));rawPiles_={};request("admin.pile.list");refreshDashboard();return;}
    else if(m.type=="admin.user.status.ok"){showNotice(QStringLiteral("用户状态已更新"));request("admin.user.list",{{"phone",userQuery_}});return;}
    else if(m.type=="admin.pricing.get.ok"){pricingDetail_=m.payload.toVariantMap();emit pricingChanged();return;}
    else if(m.type=="admin.pricing.set.ok"){showNotice(QStringLiteral("计价规则已更新,保存后立即对后续充电计费生效"));return;}
    const bool stationsRefreshed = m.type == QStringLiteral("admin.station.list.ok");
    applyClientFilters();
    // 电桩页"所属电站"下拉等依赖电站原始数据,变化时通知 QML 重新拉取
    if (stationsRefreshed) emit stationDataChanged();
}

void AdminAppController::applyClientFilters(){QJsonArray out;
    for(const auto&v:rawStations_){auto o=v.toObject();
        if(!containsCI(o,{"name","address"},stationQuery_))continue;
        // 行政区划按字段精确匹配,替代旧的地址子串猜测式过滤
        if(!regionLevelMatches(o.value("province").toString(),stationProvince_))continue;
        if(!regionLevelMatches(o.value("city").toString(),stationCity_))continue;
        if(!regionLevelMatches(o.value("district").toString(),stationDistrict_))continue;
        out.append(o);}stations_.setJson(out);out={};
    for(const auto&v:rawPiles_){auto o=v.toObject();if(!containsCI(o,{"code"},pileQuery_))continue;
        if(!pileStationIds_.isEmpty()&&!pileStationIds_.contains(o.value("station_id").toVariant().toLongLong()))continue;
        if(!pileType_.isEmpty()&&o.value("type").toString()!=pileType_)continue;if(!pileState_.isEmpty()&&o.value("status").toString()!=pileState_)continue;out.append(o);}piles_.setJson(out);out={};
    for(const auto&v:rawOrders_){auto o=v.toObject();if(!containsCI(o,{"order_no","phone","pile_code","station_name"},orderQuery_))continue;if(!orderState_.isEmpty()&&o.value("status").toString()!=orderState_)continue;out.append(o);}orders_.setJson(out);out={};
    for(const auto&v:rawUsers_){auto o=v.toObject();if(!containsCI(o,{"phone"},userQuery_))continue;if(!userState_.isEmpty()&&o.value("status").toString()!=userState_)continue;out.append(o);}users_.setJson(out);
}

void AdminAppController::refreshPredictions()
{
    const int generation = ++forecastRequestGeneration_;
    predictionLoading_ = true;
    predictionSource_ = QStringLiteral("连接中");
    predictionStatus_ = QStringLiteral("正在连接预测服务…");
    predictionModelName_ = predictionMethod_ = predictionDataScope_ = predictionCaveat_ = QStringLiteral("读取中");
    emit predictionChanged();

    QString base = qEnvironmentVariable("CHARGING_ML_URL", QStringLiteral("http://127.0.0.1:8090"));
    while (base.endsWith(QLatin1Char('/'))) base.chop(1);
    QNetworkRequest req(QUrl(base + QStringLiteral("/health")));
    req.setTransferTimeout(4000);
    auto* reply = network_.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, base] {
        const auto data = reply->readAll();
        const auto error = reply->error();
        const QString errorText = reply->errorString();
        reply->deleteLater();
        if (generation != forecastRequestGeneration_) return;
        if (error != QNetworkReply::NoError) {
            usePredictionDemo(errorText);
            return;
        }
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            usePredictionDemo(QStringLiteral("健康接口返回了无效 JSON"));
            return;
        }
        const auto health = document.object();
        forecastModel_ = health.value(QStringLiteral("model")).toString();
        forecastMode_ = health.value(QStringLiteral("mode")).toString();
        forecastReplayAt_ = health.value(QStringLiteral("default_timestamp")).toString();
        forecastStationCount_ = health.value(QStringLiteral("zones")).toInt();
        forecastShiftedData_ = health.value(QStringLiteral("date_shift")).toObject()
                                   .value(QStringLiteral("data_kind")).toString() == QStringLiteral("shifted_historical");
        const auto range = health.value(QStringLiteral("data_range")).toArray();
        forecastDateRange_ = range.size() >= 2
            ? QStringLiteral("%1 至 %2").arg(range.at(0).toString().left(10), range.at(1).toString().left(10))
            : QString();

        QNetworkRequest stationRequest(QUrl(base + QStringLiteral("/stations")));
        stationRequest.setTransferTimeout(4000);
        auto* stationReply = network_.get(stationRequest);
        connect(stationReply, &QNetworkReply::finished, this, [this, stationReply, generation] {
            const auto stationData = stationReply->readAll();
            const auto stationError = stationReply->error();
            const QString stationErrorText = stationReply->errorString();
            stationReply->deleteLater();
            if (generation != forecastRequestGeneration_) return;
            if (stationError != QNetworkReply::NoError) {
                usePredictionDemo(stationErrorText);
                return;
            }
            QJsonParseError stationParseError;
            const auto stationDocument = QJsonDocument::fromJson(stationData, &stationParseError);
            const auto catalog = stationDocument.object();
            if (stationParseError.error != QJsonParseError::NoError || !stationDocument.isObject()) {
                usePredictionDemo(QStringLiteral("站点接口返回了无效 JSON"));
                return;
            }
            if (catalog.value(QStringLiteral("stations")).toArray().isEmpty()) {
                usePredictionDemo(QStringLiteral("预测服务没有可用站点"));
                return;
            }
            requestStationForecasts(catalog, generation);
        });
    });
}

// 对每个站点并发发起 POST /predict(最多 6 个),全部完成后统一汇总展示
void AdminAppController::requestStationForecasts(const QJsonObject& catalog, int generation)
{
    QString base = qEnvironmentVariable("CHARGING_ML_URL", QStringLiteral("http://127.0.0.1:8090"));
    while (base.endsWith(QLatin1Char('/'))) base.chop(1);
    const auto stations = catalog.value(QStringLiteral("stations")).toArray();
    forecastMode_ = catalog.value(QStringLiteral("mode")).toString();
    forecastReplayAt_ = catalog.value(QStringLiteral("default_timestamp")).toString();
    forecastStationCount_ = catalog.value(QStringLiteral("count")).toInt(stations.size());
    forecastShiftedData_ = catalog.value(QStringLiteral("date_shift")).toObject()
                               .value(QStringLiteral("data_kind")).toString() == QStringLiteral("shifted_historical");
    forecastAlgorithm_.clear();
    forecastLoadBasis_.clear();
    forecastWarnings_.clear();
    forecastCapacityVerified_ = true;
    pendingForecastRows_.clear();
    forecastLoadSum_[0] = forecastLoadSum_[1] = forecastLoadSum_[2] = 0.0;
    forecastOkCount_ = 0;
    forecastConfidence_ = 0.0;
    const int total = qMin(6, stations.size());
    pendingForecastCount_ = total;

    for (int i = 0; i < total; ++i) {
        const auto entry = stations.at(i).toObject();
        const QString zone = entry.value(QStringLiteral("station_id")).toVariant().toString();
        pendingForecastRows_.append(QVariantMap{
            {QStringLiteral("station_id"), entry.value(QStringLiteral("station_id")).toVariant()},
            {QStringLiteral("station_name"), entry.value(QStringLiteral("station_name")).toString(
                 QStringLiteral("预测区域 %1").arg(zone))},
            {QStringLiteral("h1"), QStringLiteral("…")}, {QStringLiteral("h6"), QStringLiteral("…")},
            {QStringLiteral("h24"), QStringLiteral("…")},
            {QStringLiteral("free"), entry.value(QStringLiteral("total_piles")).toInt()},
            {QStringLiteral("risk"), QStringLiteral("推理中")}});
        // 只读推理:请求体仅带 station_id,历史负荷/天气由服务端回退数据集
        QNetworkRequest req(QUrl(base + QStringLiteral("/predict")));
        req.setTransferTimeout(10000);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        auto* reply = network_.post(req, QJsonDocument(QJsonObject{{QStringLiteral("station_id"), entry.value(QStringLiteral("station_id"))}}).toJson());
        connect(reply, &QNetworkReply::finished, this, [this, reply, i, generation] {
            const auto data = reply->readAll();
            const bool ok = reply->error() == QNetworkReply::NoError;
            reply->deleteLater();
            if (generation != forecastRequestGeneration_) return;
            QJsonParseError parseError;
            const auto document = QJsonDocument::fromJson(data, &parseError);
            if (ok && parseError.error == QJsonParseError::NoError && document.isObject()
                && applyForecastReply(i, document.object())) {
                ++forecastOkCount_;
            } else {
                auto row = pendingForecastRows_[i];
                row[QStringLiteral("h1")] = row[QStringLiteral("h6")] = row[QStringLiteral("h24")] = QStringLiteral("—");
                row[QStringLiteral("risk")] = QStringLiteral("推理失败");
                pendingForecastRows_[i] = row;
            }
            if (--pendingForecastCount_ == 0)
                finishForecasts();
        });
    }
}

// 解析单个站点的 /predict 响应:1/6/24h 负荷点估计、预计空闲桩、
// 风险判定(由 24h 曲线的占用率峰值推出),并累加进合计
bool AdminAppController::applyForecastReply(int row, const QJsonObject& payload)
{
    const auto available = payload.value(QStringLiteral("available_piles")).toObject();
    auto pointAt = [](const QJsonObject& holder, const char* key) {
        return holder.value(QLatin1String(key)).toObject().value(QStringLiteral("point")).toDouble();
    };
    const int free1 = int(pointAt(available, "1"));

    const auto curve = payload.value(QStringLiteral("curve")).toArray();
    if (curve.isEmpty()) return false;
    double h1 = 0.0, h6 = 0.0, h24 = 0.0;
    int validPoints = 0;
    for (int i = 0; i < curve.size() && i < 24; ++i) {
        const auto point = curve.at(i).toObject();
        const double load = point.value(QStringLiteral("load_kwh")).toDouble(-1.0);
        if (!std::isfinite(load) || load < 0.0) return false;
        ++validPoints;
        h24 += load;
        if (i < 6) h6 += load;
        if (i == 0) h1 = load;
    }
    if (validPoints < 24) return false;

    // 24h 曲线里占用率最高的小时 → 高峰提示;占用率过高/无空闲桩 → 容量预警
    double peakBusy = 0.0;
    int peakHour = 0;
    QString peakTime;
    for (const auto& item : curve) {
        const auto point = item.toObject();
        const double busy = point.value(QStringLiteral("busy_ratio")).toDouble();
        if (busy > peakBusy) {
            peakBusy = busy;
            peakHour = point.value(QStringLiteral("offset")).toInt();
            const auto start = QDateTime::fromString(point.value(QStringLiteral("interval_start")).toString(), Qt::ISODate);
            peakTime = start.isValid() ? start.toString(QStringLiteral("HH:mm")) : QString();
        }
    }
    QString risk = QStringLiteral("正常");
    const bool capacityVerified = payload.value(QStringLiteral("capacity_verified")).toBool(true);
    forecastCapacityVerified_ = forecastCapacityVerified_ && capacityVerified;
    if (forecastAlgorithm_.isEmpty())
        forecastAlgorithm_ = payload.value(QStringLiteral("selected_models")).toObject()
                                 .value(QStringLiteral("energy_kwh")).toString();
    if (forecastLoadBasis_.isEmpty())
        forecastLoadBasis_ = payload.value(QStringLiteral("load_basis")).toString();
    for (const auto& warning : payload.value(QStringLiteral("warnings")).toArray()) {
        const QString text = warning.toString().trimmed();
        if (!text.isEmpty() && !forecastWarnings_.contains(text)) forecastWarnings_.append(text);
    }
    if (free1 <= 0 || peakBusy >= 0.9)
        risk = capacityVerified ? QStringLiteral("容量预警") : QStringLiteral("高负荷提示");
    else if (peakBusy >= 0.6)
        risk = peakTime.isEmpty()
            ? QStringLiteral("第 %1 小时高峰").arg(peakHour)
            : QStringLiteral("%1 高峰").arg(peakTime);

    // 置信水平 = 分位数区间宽度,如 [0.05,0.95] → 90%
    if (forecastConfidence_ <= 0.0) {
        const auto quantiles = payload.value(QStringLiteral("quantiles")).toArray();
        if (quantiles.size() >= 2)
            forecastConfidence_ = (quantiles.last().toDouble() - quantiles.first().toDouble()) * 100.0;
    }

    forecastLoadSum_[0] += h1;
    forecastLoadSum_[1] += h6;
    forecastLoadSum_[2] += h24;

    auto fmt = [](double v) { return QStringLiteral("%1 kWh").arg(v, 0, 'f', 1); };
    pendingForecastRows_[row].insert(QStringLiteral("h1"), fmt(h1));
    pendingForecastRows_[row].insert(QStringLiteral("h6"), fmt(h6));
    pendingForecastRows_[row].insert(QStringLiteral("h24"), fmt(h24));
    pendingForecastRows_[row].insert(QStringLiteral("free"), free1);
    pendingForecastRows_[row].insert(QStringLiteral("risk"), risk);
    return true;
}

// 全部站点请求结束后:刷新表格、汇总指标卡数值与状态说明
void AdminAppController::finishForecasts()
{
    predictions_.setRows(pendingForecastRows_);
    predictionLoading_ = false;
    predictionSource_ = forecastMode_ == QStringLiteral("historical_replay")
        ? QStringLiteral("历史回放模型") : QStringLiteral("模型服务");
    predictionModelName_ = forecastModel_.isEmpty() ? QStringLiteral("未提供") : forecastModel_;
    predictionMethod_ = predictionAlgorithmLabel(forecastAlgorithm_);
    predictionConfidence_ = forecastOkCount_ > 0 && forecastConfidence_ > 0.0
        ? QStringLiteral("%1").arg(qRound(forecastConfidence_))
        : QStringLiteral("—");
    if (forecastOkCount_ > 0) {
        predictionLoad1_ = QStringLiteral("%1 kWh").arg(forecastLoadSum_[0], 0, 'f', 1);
        predictionLoad6_ = QStringLiteral("%1 kWh").arg(forecastLoadSum_[1], 0, 'f', 1);
        predictionLoad24_ = QStringLiteral("%1 kWh").arg(forecastLoadSum_[2], 0, 'f', 1);
        const int shown = pendingForecastRows_.size();
        const QString replayNote = forecastMode_ == QStringLiteral("historical_replay")
            ? QStringLiteral("；回放时点 %1").arg(forecastReplayAt_.isEmpty() ? QStringLiteral("未提供") : forecastReplayAt_)
            : QString();
        predictionStatus_ = forecastOkCount_ == pendingForecastRows_.size()
            ? QStringLiteral("服务在线；当前展示 %1/%2 个可预测站点%3")
                  .arg(shown).arg(forecastStationCount_).arg(replayNote)
            : QStringLiteral("服务在线；%1/%2 个站点预测成功，其余站点请检查模型产物")
                  .arg(forecastOkCount_).arg(pendingForecastRows_.size());
        predictionDataScope_ = QStringLiteral("%1%2 · 当前汇总 %3 个站点")
            .arg(forecastShiftedData_ ? QStringLiteral("年份平移历史数据") : QStringLiteral("模型数据"),
                 forecastDateRange_.isEmpty() ? QString() : QStringLiteral(" · %1").arg(forecastDateRange_))
            .arg(shown);
        QStringList caveats;
        if (forecastShiftedData_) caveats << QStringLiteral("并非 2024/2025/2026 年实采数据");
        if (forecastLoadBasis_ == QStringLiteral("uniform_session_energy_allocation"))
            caveats << QStringLiteral("小时电量由会话电量按时长均匀分摊重建");
        if (!forecastCapacityVerified_) caveats << QStringLiteral("站点容量尚未核验");
        if (predictionConfidence_ == QStringLiteral("—")) caveats << QStringLiteral("未提供概率预测区间");
        caveats.append(forecastWarnings_);
        predictionCaveat_ = caveats.isEmpty() ? QStringLiteral("接口未返回额外限制说明") : caveats.join(QStringLiteral("；"));
    } else {
        predictionLoad1_ = predictionLoad6_ = predictionLoad24_ = QStringLiteral("—");
        predictionStatus_ = QStringLiteral("服务在线，但推理全部失败；请确认已运行 train.py 生成模型产物");
        predictionDataScope_ = QStringLiteral("模型接口在线，但没有可展示的有效预测");
        predictionCaveat_ = QStringLiteral("请检查模型产物和预测响应字段");
    }
    predictionUpdatedAt_ = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    emit predictionChanged();
}
void AdminAppController::usePredictionDemo(const QString& reason)
{
    // 模型服务不可用时，不再展示固定演示值；改用平台真实订单做只读估算。
    QList<QVariantMap> rows;
    double sum1 = 0.0, sum6 = 0.0, sum24 = 0.0;
    int sampleCount = 0;

    double globalEnergy = 0.0;
    int globalEnergyCount = 0;
    for (const auto& value : rawOrders_) {
        const auto order = value.toObject();
        const QString status = order.value(QStringLiteral("status")).toString();
        const double energy = order.value(QStringLiteral("energy_kwh")).toDouble();
        if ((status == QStringLiteral("completed") || status == QStringLiteral("awaiting_payment")) && energy > 0.0) {
            globalEnergy += energy;
            ++globalEnergyCount;
        }
    }
    const double globalAverageEnergy = globalEnergyCount > 0 ? globalEnergy / globalEnergyCount : 0.0;
    const QDateTime now = QDateTime::currentDateTimeUtc();

    for (const auto& stationValue : rawStations_) {
        const auto station = stationValue.toObject();
        const QString stationName = station.value(QStringLiteral("name")).toString();
        const int totalPiles = station.value(QStringLiteral("pile_count")).toInt();
        const int idlePiles = station.value(QStringLiteral("idle_pile_count")).toInt();

        double recentEnergy = 0.0;
        int recentOrders = 0;
        double stationEnergy = 0.0;
        int stationEnergyCount = 0;
        for (const auto& orderValue : rawOrders_) {
            const auto order = orderValue.toObject();
            if (order.value(QStringLiteral("station_name")).toString() != stationName) continue;
            const QString status = order.value(QStringLiteral("status")).toString();
            const double energy = order.value(QStringLiteral("energy_kwh")).toDouble();
            if ((status == QStringLiteral("completed") || status == QStringLiteral("awaiting_payment")) && energy > 0.0) {
                stationEnergy += energy;
                ++stationEnergyCount;
            }
            const QDateTime created = charging::core::parseTimestamp(order.value(QStringLiteral("created_at")).toString());
            if (created.isValid()) {
                const qint64 age = created.toUTC().secsTo(now);
                if (age >= 0 && age <= 24 * 3600) {
                    ++recentOrders;
                    recentEnergy += qMax(0.0, energy);
                }
            }
        }

        const double avgSessionEnergy = stationEnergyCount > 0
            ? stationEnergy / stationEnergyCount : globalAverageEnergy;
        double load24 = recentEnergy;
        if (load24 <= 0.0 && recentOrders > 0 && avgSessionEnergy > 0.0)
            load24 = recentOrders * avgSessionEnergy;
        const double h1 = load24 / 24.0;
        const double h6 = h1 * 6.0;
        sum1 += h1; sum6 += h6; sum24 += load24;
        sampleCount += recentOrders;

        QString risk = QStringLiteral("正常");
        if (totalPiles > 0 && idlePiles <= 0)
            risk = QStringLiteral("容量预警");
        else if (totalPiles > 0 && idlePiles * 4 <= totalPiles)
            risk = QStringLiteral("空闲桩偏少");
        else if (recentOrders >= qMax(3, totalPiles))
            risk = QStringLiteral("近期需求较高");

        rows.append(QVariantMap{
            {QStringLiteral("station_id"), station.value(QStringLiteral("id")).toVariant()},
            {QStringLiteral("station_name"), stationName},
            {QStringLiteral("h1"), QStringLiteral("%1 kWh").arg(h1, 0, 'f', 1)},
            {QStringLiteral("h6"), QStringLiteral("%1 kWh").arg(h6, 0, 'f', 1)},
            {QStringLiteral("h24"), QStringLiteral("%1 kWh").arg(load24, 0, 'f', 1)},
            {QStringLiteral("free"), idlePiles},
            {QStringLiteral("risk"), risk}});
    }

    predictionLoading_ = false;
    predictions_.setRows(rows);
    predictionSource_ = QStringLiteral("历史数据估算");
    predictionModelName_ = QStringLiteral("未连接 ML 服务");
    predictionMethod_ = QStringLiteral("业务订单均值估算");
    predictionDataScope_ = QStringLiteral("平台业务数据库 · %1 个站点 · 最近 24 小时订单").arg(rows.size());
    predictionCaveat_ = QStringLiteral("这是服务离线时的只读估算，不是机器学习预测结果");
    predictionLoad1_ = QStringLiteral("%1 kWh").arg(sum1, 0, 'f', 1);
    predictionLoad6_ = QStringLiteral("%1 kWh").arg(sum6, 0, 'f', 1);
    predictionLoad24_ = QStringLiteral("%1 kWh").arg(sum24, 0, 'f', 1);
    predictionConfidence_ = QStringLiteral("—");
    predictionStatus_ = QStringLiteral("模型服务不可用（%1）；当前按最近 24 小时真实订单与实时空闲桩估算，共 %2 个近期订单样本")
                            .arg(reason).arg(sampleCount);
    predictionUpdatedAt_ = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    emit predictionChanged();
}

} // namespace charging::admin
