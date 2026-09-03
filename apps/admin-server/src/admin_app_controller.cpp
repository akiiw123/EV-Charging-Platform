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
void AdminAppController::logout(){ loggedIn_=false; administrator_.clear(); emit loggedInChanged(); }
void AdminAppController::refreshAll(){ refreshDashboard(); request(QStringLiteral("admin.station.list")); request(QStringLiteral("admin.pile.list")); request(QStringLiteral("admin.order.list")); request(QStringLiteral("admin.user.list"),{{"phone",QString()}}); }
void AdminAppController::refreshDashboard(){ request(QStringLiteral("admin.dashboard")); }
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

void AdminAppController::handleResponse(const charging::core::Message& m){ if(busyCount_>0)--busyCount_;emit busyChanged(); if(m.type.endsWith(".error")){showNotice(m.payload.value("message").toString(),"error");return;}
    if(m.type=="admin.login.ok"){loggedIn_=true;administrator_=m.payload.value("administrator").toObject().value("username").toString();errorMessage_.clear();emit loggedInChanged();emit noticeChanged();refreshAll();return;}
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

void AdminAppController::refreshPredictions(){ predictionStatus_=QStringLiteral("正在连接预测服务…");emit predictionChanged(); QNetworkRequest req(QUrl(qEnvironmentVariable("CHARGING_ML_URL",QStringLiteral("http://127.0.0.1:8090"))+QStringLiteral("/stations")));auto* reply=network_.get(req);connect(reply,&QNetworkReply::finished,this,[this,reply]{auto data=reply->readAll();if(reply->error()!=QNetworkReply::NoError){QString why=reply->errorString();reply->deleteLater();usePredictionDemo(why);return;}auto doc=QJsonDocument::fromJson(data);auto list=doc.object().value("stations").toArray();if(list.isEmpty()){reply->deleteLater();usePredictionDemo(QStringLiteral("预测服务没有可用站点"));return;}QList<QVariantMap> rows;int i=0;for(const auto&v:list){if(i++>=6)break;auto o=v.toObject();QVariantMap row{{"station_id",o.value("station_id").toVariant()},{"station_name",QStringLiteral("预测区域 %1").arg(o.value("station_id").toVariant().toString())},{"h1",QStringLiteral("等待推理")},{"h6",QStringLiteral("等待推理")},{"h24",QStringLiteral("等待推理")},{"free",o.value("pile_total").toInt()},{"risk",QStringLiteral("正常")}};rows.append(row);}predictions_.setRows(rows);predictionSource_=QStringLiteral("模型服务");predictionStatus_=QStringLiteral("服务在线；选择站点后可请求预测");predictionUpdatedAt_=QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");emit predictionChanged();reply->deleteLater();});}
void AdminAppController::usePredictionDemo(const QString& reason){QList<QVariantMap> rows={{{"station_id",1},{"station_name",QStringLiteral("深圳演示充电站")},{"h1","42.6 kWh"},{"h6","238.4 kWh"},{"h24","886.1 kWh"},{"free",3},{"risk",QStringLiteral("18:00 高峰")}},{{"station_id",2},{"station_name",QStringLiteral("南山科技园站")},{"h1","28.9 kWh"},{"h6","174.2 kWh"},{"h24","641.5 kWh"},{"free",5},{"risk",QStringLiteral("正常")}},{{"station_id",3},{"station_name",QStringLiteral("宝安中心站")},{"h1","51.3 kWh"},{"h6","302.8 kWh"},{"h24","1024.7 kWh"},{"free",2},{"risk",QStringLiteral("容量预警")}}};predictions_.setRows(rows);predictionSource_=QStringLiteral("演示数据");predictionStatus_=QStringLiteral("预测服务未就绪：%1").arg(reason);predictionUpdatedAt_=QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");emit predictionChanged();}

} // namespace charging::admin
