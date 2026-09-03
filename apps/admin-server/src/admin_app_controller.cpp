#include "admin_app_controller.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace charging::admin {
namespace {
bool containsCI(const QJsonObject& o, const QStringList& keys, const QString& needle) {
    if (needle.trimmed().isEmpty()) return true;
    for (const auto& key : keys) if (o.value(key).toVariant().toString().contains(needle, Qt::CaseInsensitive)) return true;
    return false;
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
    clock_.setInterval(1000);
    connect(&clock_, &QTimer::timeout, this, [this] { currentTime_ = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd  HH:mm:ss")); emit currentTimeChanged(); });
    currentTime_ = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd  HH:mm:ss"));
    clock_.start();
    connect(&api_, &charging::core::ApiClient::connected, this, [this] { connected_ = true; emit connectionChanged(); });
    connect(&api_, &charging::core::ApiClient::disconnected, this, [this] { connected_ = false; loggedIn_ = false; busyCount_ = 0; emit connectionChanged(); emit loggedInChanged(); emit busyChanged(); });
    connect(&api_, &charging::core::ApiClient::clientError, this, [this](const QString& text) { busyCount_ = 0; emit busyChanged(); showNotice(text, QStringLiteral("error")); });
    connect(&api_, &charging::core::ApiClient::responseReceived, this, &AdminAppController::handleResponse);
    bool ok=false; const int port=qEnvironmentVariableIntValue("CHARGING_SERVER_PORT",&ok);
    api_.connectToServer(qEnvironmentVariable("CHARGING_SERVER_HOST", QStringLiteral("127.0.0.1")), ok && port>0 ? quint16(port) : quint16(45454));
    const QString smokePassword = qEnvironmentVariable("CHARGING_ADMIN_SMOKE_PASSWORD");
    if (!smokePassword.isEmpty()) {
        QTimer::singleShot(600, this, [this, smokePassword] { login(QStringLiteral("admin"), smokePassword, false); });
    }
}

void AdminAppController::request(const QString& type, const QJsonObject& payload) { if (!api_.isConnected()) { showNotice(QStringLiteral("服务未连接，请稍后重试"), QStringLiteral("error")); return; } ++busyCount_; emit busyChanged(); api_.send(type,payload); }
void AdminAppController::login(const QString& username,const QString& password,bool remember) { errorMessage_.clear(); if(username.trimmed().isEmpty()||password.isEmpty()){errorMessage_=QStringLiteral("请输入管理员账号和密码");emit noticeChanged();return;} settings_.setValue(QStringLiteral("login/username"),remember?username:QString()); request(QStringLiteral("admin.login"),{{"username",username},{"password",password}}); }
void AdminAppController::logout(){ loggedIn_=false; administrator_.clear(); if(mustChangePassword_){mustChangePassword_=false;emit mustChangePasswordChanged();} emit loggedInChanged(); }
void AdminAppController::changePassword(const QString& oldPassword,const QString& newPassword)
{
    // 前端先行做强度/一致性检查,服务端仍会二次校验(PBKDF2 验证旧密码)
    if (newPassword.size() < 8) { showNotice(QStringLiteral("新密码至少需要 8 位"), QStringLiteral("error")); emit passwordChangeResult(false); return; }
    request(QStringLiteral("admin.password.change"),{{QStringLiteral("old_password"),oldPassword},{QStringLiteral("new_password"),newPassword}});
}
void AdminAppController::refreshAll(){ refreshDashboard(); request(QStringLiteral("admin.station.list")); request(QStringLiteral("admin.pile.list")); request(QStringLiteral("admin.order.list")); request(QStringLiteral("admin.user.list"),{{"phone",QString()}}); }
void AdminAppController::refreshDashboard(int days)
{
    // 请求指定区间的运营总览,服务端按 days(7/30)返回营收趋势
    request(QStringLiteral("admin.dashboard"), {{QStringLiteral("days"), days}});
}
void AdminAppController::refreshStations(const QString& q){ stationQuery_=q; applyClientFilters(); if(rawStations_.isEmpty())request(QStringLiteral("admin.station.list")); }
void AdminAppController::refreshPiles(const QString& q,const QString& station,const QString& type,const QString& status){pileQuery_=q;pileStation_=station;pileType_=type;pileState_=status;applyClientFilters();if(rawPiles_.isEmpty())request(QStringLiteral("admin.pile.list"));}
void AdminAppController::refreshOrders(const QString& q,const QString& status){orderQuery_=q;orderState_=status;applyClientFilters();if(rawOrders_.isEmpty())request(QStringLiteral("admin.order.list"));}
void AdminAppController::refreshUsers(const QString& q,const QString& status){userQuery_=q;userState_=status;request(QStringLiteral("admin.user.list"),{{"phone",q}});}
void AdminAppController::createStation(const QVariantMap& f){request(QStringLiteral("admin.station.create"),QJsonObject::fromVariantMap(f));}
void AdminAppController::updateStation(const QVariantMap& f){request(QStringLiteral("admin.station.update"),QJsonObject::fromVariantMap(f));}
void AdminAppController::deleteStation(qint64 id){request(QStringLiteral("admin.station.delete"),{{"station_id",id}});}
void AdminAppController::restartPile(qint64 id){request(QStringLiteral("admin.pile.restart"),{{"pile_id",id}});}
void AdminAppController::setUserStatus(qint64 id,const QString& status){request(QStringLiteral("admin.user.status"),{{"user_id",id},{"status",status}});}
void AdminAppController::clearNotice(){notice_.clear();noticeKind_.clear();emit noticeChanged();}
QString AdminAppController::savedUsername() const{return settings_.value(QStringLiteral("login/username")).toString();}
void AdminAppController::setTheme(const QString& v){if(theme_==v)return;theme_=v;settings_.setValue("appearance/theme",v);emit themeChanged();}
void AdminAppController::setSidebarExpanded(bool v){if(sidebarExpanded_==v)return;sidebarExpanded_=v;settings_.setValue("appearance/sidebarExpanded",v);emit settingsChanged();}
void AdminAppController::setAnimationsEnabled(bool v){if(animationsEnabled_==v)return;animationsEnabled_=v;settings_.setValue("appearance/animations",v);emit settingsChanged();}
void AdminAppController::setFontScale(double v){v=qBound(.85,v,1.3);if(qFuzzyCompare(fontScale_,v))return;fontScale_=v;settings_.setValue("appearance/fontScale",v);emit settingsChanged();}
void AdminAppController::setPageSize(int v){v=qBound(10,v,100);if(pageSize_==v)return;pageSize_=v;settings_.setValue("table/pageSize",v);emit settingsChanged();}
void AdminAppController::showNotice(const QString&t,const QString&k){notice_=t;noticeKind_=k;if(k=="error")errorMessage_=t;emit noticeChanged();}

void AdminAppController::handleResponse(const charging::core::Message& m){ if(busyCount_>0)--busyCount_;emit busyChanged(); if(m.type.endsWith(".error")){showNotice(m.payload.value("message").toString(),"error");if(m.type==QStringLiteral("admin.password.change.error"))emit passwordChangeResult(false);return;}
    if(m.type=="admin.login.ok"){loggedIn_=true;administrator_=m.payload.value("administrator").toObject().value("username").toString();const bool mustChange=m.payload.value("administrator").toObject().value("must_change_password").toBool();if(mustChange!=mustChangePassword_){mustChangePassword_=mustChange;emit mustChangePasswordChanged();}errorMessage_.clear();emit loggedInChanged();emit noticeChanged();refreshAll();return;}
    if(m.type=="admin.password.change.ok"){mustChangePassword_=false;emit mustChangePasswordChanged();showNotice(QStringLiteral("密码已更新，请牢记新密码"));emit passwordChangeResult(true);return;}
    if(m.type=="admin.dashboard.ok"){dashboard_=m.payload.value("metrics").toObject().toVariantMap();pileStatus_=m.payload.value("pile_status").toObject().toVariantMap();revenueTrend_=m.payload.value("revenue_trend").toArray().toVariantList();stationEnergy_=m.payload.value("station_energy").toArray().toVariantList();emit dashboardChanged();return;}
    if(m.type=="admin.station.list.ok")rawStations_=m.payload.value("stations").toArray();
    else if(m.type=="admin.pile.list.ok")rawPiles_=m.payload.value("piles").toArray();
    else if(m.type=="admin.order.list.ok")rawOrders_=m.payload.value("orders").toArray();
    else if(m.type=="admin.user.list.ok")rawUsers_=m.payload.value("users").toArray();
    else if(m.type=="admin.station.create.ok"||m.type=="admin.station.update.ok"||m.type=="admin.station.delete.ok"){showNotice(QStringLiteral("电站信息已更新"));rawStations_={};request("admin.station.list");request("admin.pile.list");refreshDashboard();return;}
    else if(m.type=="admin.pile.restart.ok"){showNotice(QStringLiteral("重启指令执行成功"));rawPiles_={};request("admin.pile.list");refreshDashboard();return;}
    else if(m.type=="admin.user.status.ok"){showNotice(QStringLiteral("用户状态已更新"));request("admin.user.list",{{"phone",userQuery_}});return;}
    applyClientFilters();
}

void AdminAppController::applyClientFilters(){QJsonArray out;
    for(const auto&v:rawStations_){auto o=v.toObject();if(containsCI(o,{"name","address"},stationQuery_))out.append(o);}stations_.setJson(out);out={};
    for(const auto&v:rawPiles_){auto o=v.toObject();if(!containsCI(o,{"code"},pileQuery_))continue;if(!pileStation_.isEmpty()&&o.value("station_name").toString()!=pileStation_)continue;if(!pileType_.isEmpty()&&o.value("type").toString()!=pileType_)continue;if(!pileState_.isEmpty()&&o.value("status").toString()!=pileState_)continue;out.append(o);}piles_.setJson(out);out={};
    for(const auto&v:rawOrders_){auto o=v.toObject();if(!containsCI(o,{"order_no","phone","pile_code","station_name"},orderQuery_))continue;if(!orderState_.isEmpty()&&o.value("status").toString()!=orderState_)continue;out.append(o);}orders_.setJson(out);out={};
    for(const auto&v:rawUsers_){auto o=v.toObject();if(!userState_.isEmpty()&&o.value("status").toString()!=userState_)continue;out.append(o);}users_.setJson(out);
}

void AdminAppController::refreshPredictions(){ predictionStatus_=QStringLiteral("正在连接预测服务…");emit predictionChanged(); QNetworkRequest req(QUrl(qEnvironmentVariable("CHARGING_ML_URL",QStringLiteral("http://127.0.0.1:8090"))+QStringLiteral("/stations")));req.setTransferTimeout(4000);auto* reply=network_.get(req);connect(reply,&QNetworkReply::finished,this,[this,reply]{auto data=reply->readAll();if(reply->error()!=QNetworkReply::NoError){QString why=reply->errorString();reply->deleteLater();usePredictionDemo(why);return;}auto list=QJsonDocument::fromJson(data).object().value("stations").toArray();reply->deleteLater();if(list.isEmpty()){usePredictionDemo(QStringLiteral("预测服务没有可用站点"));return;}requestStationForecasts(list);});}

// 对每个站点并发发起 POST /predict(最多 6 个),全部完成后统一汇总展示
void AdminAppController::requestStationForecasts(const QJsonArray& stations)
{
    const QString base = qEnvironmentVariable("CHARGING_ML_URL", QStringLiteral("http://127.0.0.1:8090"));
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
            {QStringLiteral("station_name"), QStringLiteral("预测区域 %1").arg(zone)},
            {QStringLiteral("h1"), QStringLiteral("…")}, {QStringLiteral("h6"), QStringLiteral("…")},
            {QStringLiteral("h24"), QStringLiteral("…")},
            {QStringLiteral("free"), entry.value(QStringLiteral("total_piles")).toInt()},
            {QStringLiteral("risk"), QStringLiteral("推理中")}});
        // 只读推理:请求体仅带 station_id,历史负荷/天气由服务端回退数据集
        QNetworkRequest req(QUrl(base + QStringLiteral("/predict")));
        req.setTransferTimeout(10000);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        auto* reply = network_.post(req, QJsonDocument(QJsonObject{{QStringLiteral("station_id"), entry.value(QStringLiteral("station_id"))}}).toJson());
        connect(reply, &QNetworkReply::finished, this, [this, reply, i] {
            const auto data = reply->readAll();
            const bool ok = reply->error() == QNetworkReply::NoError;
            reply->deleteLater();
            if (ok) {
                applyForecastReply(i, QJsonDocument::fromJson(data).object());
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
void AdminAppController::applyForecastReply(int row, const QJsonObject& payload)
{
    const auto loadKwh = payload.value(QStringLiteral("load_kwh")).toObject();
    const auto available = payload.value(QStringLiteral("available_piles")).toObject();
    auto pointAt = [](const QJsonObject& holder, const char* key) {
        return holder.value(QLatin1String(key)).toObject().value(QStringLiteral("point")).toDouble();
    };
    const double h1 = pointAt(loadKwh, "1"), h6 = pointAt(loadKwh, "6"), h24 = pointAt(loadKwh, "24");
    const int free1 = int(pointAt(available, "1"));

    // 24h 曲线里占用率最高的小时 → 高峰提示;占用率过高/无空闲桩 → 容量预警
    double peakBusy = 0.0;
    int peakHour = 0;
    for (const auto& item : payload.value(QStringLiteral("curve")).toArray()) {
        const auto point = item.toObject();
        const double busy = point.value(QStringLiteral("busy_ratio")).toDouble();
        if (busy > peakBusy) {
            peakBusy = busy;
            peakHour = point.value(QStringLiteral("offset")).toInt();
        }
    }
    QString risk = QStringLiteral("正常");
    if (free1 <= 0 || peakBusy >= 0.9)
        risk = QStringLiteral("容量预警");
    else if (peakBusy >= 0.6)
        risk = QStringLiteral("%1:00 高峰").arg(peakHour, 2, 10, QLatin1Char('0'));

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
}

// 全部站点请求结束后:刷新表格、汇总指标卡数值与状态说明
void AdminAppController::finishForecasts()
{
    predictions_.setRows(pendingForecastRows_);
    predictionSource_ = QStringLiteral("模型服务");
    predictionConfidence_ = forecastOkCount_ > 0
        ? QStringLiteral("%1").arg(qRound(forecastConfidence_))
        : QStringLiteral("—");
    if (forecastOkCount_ > 0) {
        predictionLoad1_ = QStringLiteral("%1 kWh").arg(forecastLoadSum_[0], 0, 'f', 1);
        predictionLoad6_ = QStringLiteral("%1 kWh").arg(forecastLoadSum_[1], 0, 'f', 1);
        predictionLoad24_ = QStringLiteral("%1 kWh").arg(forecastLoadSum_[2], 0, 'f', 1);
        predictionStatus_ = forecastOkCount_ == pendingForecastRows_.size()
            ? QStringLiteral("服务在线；已完成 %1 个站点的负荷预测").arg(forecastOkCount_)
            : QStringLiteral("服务在线；%1/%2 个站点预测成功，其余站点请检查模型产物")
                  .arg(forecastOkCount_).arg(pendingForecastRows_.size());
    } else {
        predictionLoad1_ = predictionLoad6_ = predictionLoad24_ = QStringLiteral("—");
        predictionStatus_ = QStringLiteral("服务在线，但推理全部失败；请确认已运行 train.py 生成模型产物");
    }
    predictionUpdatedAt_ = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    emit predictionChanged();
}
void AdminAppController::usePredictionDemo(const QString& reason){predictionLoad1_=predictionLoad6_=predictionLoad24_=QStringLiteral("—");predictionConfidence_=QStringLiteral("—");QList<QVariantMap> rows={{{"station_id",1},{"station_name",QStringLiteral("深圳演示充电站")},{"h1","42.6 kWh"},{"h6","238.4 kWh"},{"h24","886.1 kWh"},{"free",3},{"risk",QStringLiteral("18:00 高峰")}},{{"station_id",2},{"station_name",QStringLiteral("南山科技园站")},{"h1","28.9 kWh"},{"h6","174.2 kWh"},{"h24","641.5 kWh"},{"free",5},{"risk",QStringLiteral("正常")}},{{"station_id",3},{"station_name",QStringLiteral("宝安中心站")},{"h1","51.3 kWh"},{"h6","302.8 kWh"},{"h24","1024.7 kWh"},{"free",2},{"risk",QStringLiteral("容量预警")}}};predictions_.setRows(rows);predictionSource_=QStringLiteral("演示数据");predictionStatus_=QStringLiteral("预测服务未就绪：%1").arg(reason);predictionUpdatedAt_=QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");emit predictionChanged();}

} // namespace charging::admin
