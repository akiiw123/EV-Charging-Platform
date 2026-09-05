#include "user_app_controller.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QtMath>
#include <algorithm>

namespace charging::user {

UserAppController::UserAppController(QObject* parent) : QObject(parent)
{
    requestTimer_.setSingleShot(true);
    requestTimer_.setInterval(15000);
    connect(&requestTimer_, &QTimer::timeout, this, [this] {
        clearSession();
        showNotice(QStringLiteral("请求超时，请重新登录核对结果后再操作"), QStringLiteral("error"));
    });
    chargingTimer_.setInterval(1000);
    connect(&chargingTimer_, &QTimer::timeout, this, [this] {
        ++chargingSeconds_;
        updateChargingEstimate();
    });
    noticeTimer_.setSingleShot(true);
    noticeTimer_.setInterval(4500);
    connect(&noticeTimer_, &QTimer::timeout, this, &UserAppController::clearNotice);
    connect(&api_, &charging::core::ApiClient::connected, this, [this] {
        connected_ = true;
        emit connectedChanged();
        showNotice(QStringLiteral("已连接充电服务"), QStringLiteral("success"));
    });
    connect(&api_, &charging::core::ApiClient::disconnected, this, [this] {
        connected_ = false;
        emit connectedChanged();
        clearSession();
        showNotice(QStringLiteral("连接已断开，请重新登录；若账号被冻结请联系管理员"), QStringLiteral("warning"));
    });
    connect(&api_, &charging::core::ApiClient::clientError, this,
            [this](const QString& error) { showNotice(error, QStringLiteral("error")); });
    connect(&api_, &charging::core::ApiClient::responseReceived,
            this, &UserAppController::handleResponse);
    // 过期响应不更新界面,仅复位 busy
    connect(&api_, &charging::core::ApiClient::staleResponseReceived, this, [this](const charging::core::Message& message) { pending_.remove(message.id); if (pending_.isEmpty()) requestTimer_.stop(); });

    const QString host = qEnvironmentVariable(
        "CHARGING_SERVER_HOST", QStringLiteral("127.0.0.1"));
    bool portOk = false;
    const int configuredPort =
        qEnvironmentVariableIntValue("CHARGING_SERVER_PORT", &portOk);
    api_.connectToServer(
        host, portOk && configuredPort > 0 ? quint16(configuredPort) : quint16(45454));
    updateChargingEstimate();
}

bool UserAppController::connected() const { return connected_; }
bool UserAppController::loggedIn() const { return loggedIn_; }
bool UserAppController::busy() const { return busy_; }
QString UserAppController::notice() const { return notice_; }
QString UserAppController::noticeKind() const { return noticeKind_; }
QString UserAppController::lastPhone() const
{
    QSettings settings;
    return settings.value(QStringLiteral("user/lastPhone")).toString();
}
QVariantMap UserAppController::user() const { return user_; }
QVariantMap UserAppController::activeOrder() const { return activeOrder_; }
QVariantMap UserAppController::selectedStation() const { return selectedStation_; }
QVariantList UserAppController::stations() const { return stations_; }
QVariantList UserAppController::piles() const { return piles_; }
QVariantList UserAppController::history() const { return history_; }
QVariantList UserAppController::rechargeHistory() const
{
    return rechargeHistory_;
}
QString UserAppController::locationName() const { return locationName_; }
double UserAppController::latitude() const { return latitude_; }
double UserAppController::longitude() const { return longitude_; }
QString UserAppController::searchQuery() const { return searchQuery_; }
QString UserAppController::chargingEstimate() const { return chargingEstimate_; }
QUrl UserAppController::mapUrl() const { return mapUrl_; }
QString UserAppController::mapTitle() const { return mapTitle_; }

void UserAppController::setSearchQuery(const QString& value)
{
    if (searchQuery_ == value) return;
    searchQuery_ = value.trimmed();
    emit searchQueryChanged();
    rebuildStations();
}

void UserAppController::setBusy(bool value)
{
    if (busy_ == value) return;
    busy_ = value;
    emit busyChanged();
}

void UserAppController::showNotice(const QString& text, const QString& kind)
{
    notice_ = text;
    noticeKind_ = kind;
    emit noticeChanged();
    noticeTimer_.start();
}

void UserAppController::clearNotice()
{
    if (notice_.isEmpty()) return;
    notice_.clear();
    emit noticeChanged();
}

void UserAppController::login(const QString& phone)
{
    if (busy_) return;
    static const QRegularExpression pattern(QStringLiteral("^1[3-9]\\d{9}$"));
    if (!pattern.match(phone.trimmed()).hasMatch()) {
        showNotice(QStringLiteral("请输入正确的 11 位手机号"), QStringLiteral("error"));
        emit authenticationRejected();
        return;
    }
    if (!connected_) {
        showNotice(QStringLiteral("服务尚未连接，请稍后重试"), QStringLiteral("warning"));
        return;
    }
    setBusy(true);
    sendRequest(QStringLiteral("auth.phone_login"),
              {{QStringLiteral("phone"), phone.trimmed()}});
}

void UserAppController::clearSession()
{
    ++session_;
    pending_.clear(); requestTimer_.stop(); locating_ = false; emit locatingChanged();
    loggedIn_ = false;
    user_.clear(); activeOrder_.clear(); selectedStation_.clear();
    rawStations_.clear(); stations_.clear(); piles_.clear(); history_.clear(); rechargeHistory_.clear();
    stationPiles_.clear(); filterQueue_.clear(); loadingStation_ = 0;
    mapUrl_ = QUrl(); mapTitle_.clear(); searchQuery_.clear();
    minDistance_ = minPrice_ = 0; maxDistance_ = maxPrice_ = -1; pileType_.clear(); idleOnly_ = false;
    chargingTimer_.stop(); chargingSeconds_ = 0; selectedPowerKw_ = selectedPrice_ = 0;
    locationName_ = QStringLiteral("北京理工大学良乡校区"); latitude_ = 39.7296; longitude_ = 116.1710;
    setBusy(false); updateChargingEstimate();
    emit loggedInChanged(); emit userChanged(); emit activeOrderChanged(); emit selectedStationChanged();
    emit stationsChanged(); emit pilesChanged(); emit historyChanged(); emit rechargeHistoryChanged();
    emit mapChanged(); emit searchQueryChanged(); emit locationChanged();
}

void UserAppController::logout()
{
    clearSession();
    showNotice(QStringLiteral("已安全退出"), QStringLiteral("success"));
}

QString UserAppController::sendRequest(const QString& type, const QJsonObject& payload)
{
    if (!connected_ || (!loggedIn_ && type != QStringLiteral("auth.phone_login"))) {
        setBusy(false);
        showNotice(QStringLiteral("请连接服务并登录后重试"), QStringLiteral("error"));
        return {};
    }
    if (pending_.values().contains(type)) return {};
    const QString id = api_.send(type, payload);
    if (!id.isEmpty()) { pending_.insert(id, type); if (!requestTimer_.isActive()) requestTimer_.start(); }
    else setBusy(false);
    return id;
}

QString UserAppController::orderStatusText(const QString& status) const
{
    if (status == QStringLiteral("reserved")) return QStringLiteral("已预约");
    if (status == QStringLiteral("charging")) return QStringLiteral("充电中");
    if (status == QStringLiteral("awaiting_payment")) return QStringLiteral("待结算");
    if (status == QStringLiteral("completed")) return QStringLiteral("已完成");
    if (status == QStringLiteral("cancelled")) return QStringLiteral("已取消");
    return QStringLiteral("未知状态");
}

void UserAppController::setFilters(double minDistance, double maxDistance, double minPrice, double maxPrice, const QString& type, bool idleOnly)
{
    if (!qIsFinite(minDistance) || !qIsFinite(maxDistance) || !qIsFinite(minPrice) || !qIsFinite(maxPrice)
        || minDistance < 0 || minPrice < 0 || (maxDistance != -1 && maxDistance < minDistance)
        || (maxPrice != -1 && maxPrice < minPrice)
        || (!type.isEmpty() && type != QStringLiteral("fast") && type != QStringLiteral("slow"))) {
        showNotice(QStringLiteral("筛选范围无效：上限不能小于下限"), QStringLiteral("error")); return;
    }
    minDistance_ = minDistance; maxDistance_ = maxDistance;
    minPrice_ = minPrice; maxPrice_ = maxPrice; pileType_ = type; idleOnly_ = idleOnly;
    rebuildStations();
}

QVariantMap UserAppController::filters() const
{
    return {{QStringLiteral("minDistance"), minDistance_}, {QStringLiteral("maxDistance"), maxDistance_},
        {QStringLiteral("minPrice"), minPrice_}, {QStringLiteral("maxPrice"), maxPrice_},
        {QStringLiteral("type"), pileType_}, {QStringLiteral("idleOnly"), idleOnly_}};
}

void UserAppController::loadNextFilterPiles()
{
    if (loadingStation_ || filterQueue_.isEmpty()) return;
    loadingStation_ = filterQueue_.takeFirst();
    sendRequest(QStringLiteral("pile.list"), {{QStringLiteral("station_id"), loadingStation_}});
}

void UserAppController::refreshStations()
{
    if (loggedIn_) sendRequest(QStringLiteral("station.list"));
}

// 内置城市坐标表:相当于"模拟 GPS/区域选择",离线可用
static const char* kPresetCityKeys[] = {
    "北京", "上海", "广州", "深圳", "沈阳", "杭州"
};
static const double kPresetCityCoords[][3] = {
    {39.7296, 116.1710},   // 北京:北理工良乡校区(课程演示)
    {31.2304, 121.4737},   // 上海
    {23.1291, 113.2644},   // 广州
    {22.5431, 114.0579},   // 深圳
    {41.8057, 123.4315},   // 沈阳
    {30.2741, 120.1551},   // 杭州
};

void UserAppController::locate(const QString& address)
{
    if (locating_) return;
    const QString text = address.trimmed();
    if (text.isEmpty()) {
        showNotice(QStringLiteral("请输入城市或地址"), QStringLiteral("error"));
        return;
    }
    // 1) 命中内置城市:即时定位,不依赖网络
    for (int i = 0; i < 6; ++i) {
        const QString city = QString::fromUtf8(kPresetCityKeys[i]);
        if (text == city || text == city + QStringLiteral("市")) {
            QString displayName = city + QStringLiteral("市");
            // 北京快捷定位用于课程演示,定位到北京理工大学良乡校区
            if (city == QStringLiteral("北京")) {
                displayName = QStringLiteral("北京理工大学良乡校区");
            }
            applyLocation(displayName, kPresetCityCoords[i][0], kPresetCityCoords[i][1]);
            return;
        }
    }
    // 2) 其他地址:配置了地图 Key 则真实地理编码
    if (mapKeyConfigured()) {
        geocodeAddress(text);
        return;
    }
    // 3) 降级:无 Key 时保持当前定位并说明原因
    showNotice(QStringLiteral("未配置 TENCENT_MAP_KEY,暂只支持内置城市定位(北京/上海/广州/深圳/沈阳/杭州)"),
               QStringLiteral("error"));
}

// 统一的定位落地:写状态、持久化、触发按距离重排
void UserAppController::applyLocation(const QString& name, double latitude, double longitude)
{
    locationName_ = name;
    latitude_ = latitude;
    longitude_ = longitude;
    QSettings settings;
    settings.setValue(QStringLiteral("location/name"), locationName_);
    settings.setValue(QStringLiteral("location/latitude"), latitude_);
    settings.setValue(QStringLiteral("location/longitude"), longitude_);
    emit locationChanged();
    rebuildStations();
    showNotice(QStringLiteral("已定位到 %1").arg(locationName_), QStringLiteral("success"));
}

// 腾讯 WebService 地理编码:address -> 经纬度(Key 从环境变量读取,不入库不打包)
void UserAppController::geocodeAddress(const QString& address)
{
    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/geocoder/v1/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("address"), address);
    query.addQueryItem(QStringLiteral("key"), qEnvironmentVariable("TENCENT_MAP_KEY"));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setTransferTimeout(5000);
    locating_ = true;
    emit locatingChanged();
    const quint64 generation = session_;
    auto* reply = network_.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, address, generation] {
        const auto data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        reply->deleteLater();
        if (generation != session_) return;
        locating_ = false;
        emit locatingChanged();
        if (!ok) {
            showNotice(QStringLiteral("定位服务不可达,保持当前定位"), QStringLiteral("error"));
            return;
        }
        const auto result = QJsonDocument::fromJson(data).object();
        if (!result.value(QStringLiteral("status")).isDouble() || result.value(QStringLiteral("status")).toInt() != 0) {
            showNotice(QStringLiteral("地址解析失败: %1")
                           .arg(result.value(QStringLiteral("message")).toString()),
                       QStringLiteral("error"));
            return;
        }
        const auto location = result.value(QStringLiteral("result")).toObject()
                                  .value(QStringLiteral("location")).toObject();
        const auto lat = location.value(QStringLiteral("lat"));
        const auto lng = location.value(QStringLiteral("lng"));
        if (!lat.isDouble() || !lng.isDouble() || !qIsFinite(lat.toDouble()) || !qIsFinite(lng.toDouble())
            || qAbs(lat.toDouble()) > 90 || qAbs(lng.toDouble()) > 180) {
            showNotice(QStringLiteral("地址解析失败，保持原输入和原定位"), QStringLiteral("error"));
            return;
        }
        applyLocation(address, location.value(QStringLiteral("lat")).toDouble(),
                      location.value(QStringLiteral("lng")).toDouble());
    });
}

void UserAppController::selectStation(const QVariantMap& station)
{
    selectedStation_ = station;
    selectedPrice_ = station.value(QStringLiteral("price_per_kwh")).toDouble();
    piles_.clear();
    emit selectedStationChanged();
    emit pilesChanged();
    loadPiles(station.value(QStringLiteral("id")).toLongLong());
}

void UserAppController::loadPiles(qint64 stationId)
{
    if (stationId <= 0) return;
    if (stationPiles_.contains(stationId)) { piles_ = stationPiles_.value(stationId); emit pilesChanged(); }
    if (loadingStation_ != stationId && !filterQueue_.contains(stationId)) filterQueue_.prepend(stationId);
    loadNextFilterPiles();
}

void UserAppController::reserve(qint64 pileId, double powerKw)
{
    if (busy_) return;
    if (!activeOrder_.isEmpty()) {
        showNotice(QStringLiteral("请先处理当前订单"), QStringLiteral("warning"));
        return;
    }
    selectedPowerKw_ = powerKw;
    setBusy(true);
    sendRequest(QStringLiteral("order.reserve"),
              {{QStringLiteral("pile_id"), pileId}});
}

void UserAppController::orderAction(const QString& action)
{
    if (busy_) return;
    const qint64 orderId = activeOrder_.value(QStringLiteral("id")).toLongLong();
    const QString status = activeOrder_.value(QStringLiteral("status")).toString();
    if (orderId <= 0 || !((status == QStringLiteral("reserved") && (action == QStringLiteral("order.start") || action == QStringLiteral("order.cancel")))
        || (status == QStringLiteral("charging") && action == QStringLiteral("order.stop"))
        || (status == QStringLiteral("awaiting_payment") && action == QStringLiteral("order.settle")))) {
        showNotice(QStringLiteral("当前订单状态不支持此操作，请刷新后重试"), QStringLiteral("error")); return;
    }
    setBusy(true);
    sendRequest(action, {{QStringLiteral("order_id"), orderId}});
}

void UserAppController::refreshProfile()
{
    if (!loggedIn_) return;

    sendRequest(QStringLiteral("user.profile"));
    sendRequest(QStringLiteral("order.active"));
    sendRequest(QStringLiteral("order.history"));

    // 查询当前用户充值记录
    sendRequest(QStringLiteral("recharge.history"));
}

void UserAppController::updateNickname(const QString& nickname)
{
    if (busy_) return;
    if (nickname.trimmed().isEmpty() || nickname.trimmed().toUcs4().size() > 30) {
        showNotice(QStringLiteral("昵称须为 1 至 30 字"), QStringLiteral("error"));
        return;
    }
    setBusy(true);
    sendRequest(QStringLiteral("user.profile.update"),
              {{QStringLiteral("nickname"), nickname.trimmed()}});
}

// 头像更换:系统文件选择器 → 校验 → 居中裁方缩放 256×256 → 圆形透明 PNG
// 存入应用数据目录(避免原文件被移动/删除后头像失效)→ 上传路径给服务端。
// 服务端仅保存路径字符串;本客户端读取本地文件渲染,预览即时生效。
void UserAppController::pickAvatar()
{
    if (busy_) return;
    const QString source = QFileDialog::getOpenFileName(
        nullptr, QStringLiteral("选择头像图片"), QString(),
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp)"));
    if (source.isEmpty())
        return;   // 用户取消选择
    const QFileInfo info(source);
    if (!info.isFile() || !info.isReadable()) {
        showNotice(QStringLiteral("图片文件不可读"), QStringLiteral("error"));
        return;
    }
    constexpr qint64 kMaxBytes = 5 * 1024 * 1024;   // 5 MB
    if (info.size() > kMaxBytes) {
        showNotice(QStringLiteral("图片不能超过 5 MB"), QStringLiteral("error"));
        return;
    }

    QImage image(source);
    if (image.isNull()) {
        showNotice(QStringLiteral("无法解析图片文件"), QStringLiteral("error"));
        return;
    }
    // 居中裁正方形后缩放,再按圆形裁剪输出带透明背景的 PNG,
    // 这样 QML 端直接 Image 显示即为圆形,无需特效模块
    const int side = qMin(image.width(), image.height());
    const QRect cropped((image.width() - side) / 2, (image.height() - side) / 2, side, side);
    QImage scaled = image.copy(cropped).scaled(
        256, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage rounded(256, 256, QImage::Format_RGBA8888);
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath circle;
    circle.addEllipse(0, 0, 256, 256);
    painter.setClipPath(circle);
    painter.drawImage(0, 0, scaled);
    painter.end();

    const QString avatarDir = QStandardPaths::writableLocation(
                                  QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/avatars");
    if (!QDir().mkpath(avatarDir)) {
        showNotice(QStringLiteral("头像目录创建失败"), QStringLiteral("error"));
        return;
    }
    // 按手机号命名,切换账号不串头像;QImage 只保存不移动原文件
    const QString avatarPath = avatarDir + QStringLiteral("/profile-%1.png")
        .arg(user_.value(QStringLiteral("phone")).toString());
    if (!rounded.save(avatarPath, "PNG")) {
        showNotice(QStringLiteral("头像保存失败"), QStringLiteral("error"));
        return;
    }

    // 乐观更新界面(立即预览);user.profile.update.ok 返回后 user 会再次刷新

    setBusy(true);
    sendRequest(QStringLiteral("user.profile.update"),
              {{QStringLiteral("avatar_path"), avatarPath}});
}

void UserAppController::recharge(double amount)
{
    if (busy_) return;
    if (!qIsFinite(amount) || amount <= 0.0) {
        showNotice(QStringLiteral("充值金额必须大于 0"), QStringLiteral("error"));
        return;
    }
    setBusy(true);
    sendRequest(QStringLiteral("wallet.recharge"),
              {{QStringLiteral("amount"), amount}});
}

void UserAppController::openNavigation(const QString& mode)
{
    if (selectedStation_.isEmpty()) return;
    QUrl url(QStringLiteral("https://apis.map.qq.com/uri/v1/routeplan"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("type"), mode);
    query.addQueryItem(QStringLiteral("from"), locationName_);
    query.addQueryItem(QStringLiteral("fromcoord"),
                       QStringLiteral("%1,%2").arg(latitude_, 0, 'f', 6)
                           .arg(longitude_, 0, 'f', 6));
    query.addQueryItem(
        QStringLiteral("to"),
        selectedStation_.value(QStringLiteral("name")).toString());
    query.addQueryItem(
        QStringLiteral("tocoord"),
        QStringLiteral("%1,%2")
            .arg(selectedStation_.value(QStringLiteral("latitude")).toDouble(), 0, 'f', 6)
            .arg(selectedStation_.value(QStringLiteral("longitude")).toDouble(), 0, 'f', 6));
    query.addQueryItem(QStringLiteral("referer"), QStringLiteral("charging-platform"));
    const QString key = qEnvironmentVariable("TENCENT_MAP_KEY");
    if (!key.isEmpty()) query.addQueryItem(QStringLiteral("key"), key);
    url.setQuery(query);
    mapUrl_ = url;
    mapTitle_ = (mode == QStringLiteral("walk") ? QStringLiteral("步行导航 · ")
                                                 : QStringLiteral("驾车导航 · "))
        + selectedStation_.value(QStringLiteral("name")).toString();
    emit mapChanged();
}

double UserAppController::distanceKm(
    double lat1, double lon1, double lat2, double lon2)
{
    constexpr double radius = 6371.0;
    const double latDelta = qDegreesToRadians(lat2 - lat1);
    const double lonDelta = qDegreesToRadians(lon2 - lon1);
    const double value = qSin(latDelta / 2) * qSin(latDelta / 2)
        + qCos(qDegreesToRadians(lat1)) * qCos(qDegreesToRadians(lat2))
            * qSin(lonDelta / 2) * qSin(lonDelta / 2);
    return radius * 2 * qAtan2(qSqrt(value), qSqrt(1 - value));
}

void UserAppController::rebuildStations()
{
    QVariantList filtered;
    QString currentCity;

if (locationName_.contains(QStringLiteral("北京")))
    currentCity = QStringLiteral("北京");
else if (locationName_.contains(QStringLiteral("上海")))
    currentCity = QStringLiteral("上海");
else if (locationName_.contains(QStringLiteral("广州")))
    currentCity = QStringLiteral("广州");
else if (locationName_.contains(QStringLiteral("深圳")))
    currentCity = QStringLiteral("深圳");
else if (locationName_.contains(QStringLiteral("沈阳")))
    currentCity = QStringLiteral("沈阳");
else if (locationName_.contains(QStringLiteral("杭州")))
    currentCity = QStringLiteral("杭州");
    for (const QVariant& value : rawStations_) {
        QVariantMap station = value.toMap();
        // 根据当前定位城市过滤充电站，避免深圳站出现在北京列表中
if (!currentCity.isEmpty()) {
    const QString stationAddress =
        station.value(QStringLiteral("address")).toString();
    const QString stationName =
        station.value(QStringLiteral("name")).toString();

    if (!stationAddress.contains(currentCity)
        && !stationName.contains(currentCity)) {
        continue;
    }
}
        if (!searchQuery_.isEmpty()
            && !station.value(QStringLiteral("name")).toString().contains(
                searchQuery_, Qt::CaseInsensitive)
            && !station.value(QStringLiteral("address")).toString().contains(
                searchQuery_, Qt::CaseInsensitive)) {
            continue;
        }
        station.insert(
            QStringLiteral("distance_km"),
            distanceKm(
                latitude_, longitude_,
                station.value(QStringLiteral("latitude")).toDouble(),
                station.value(QStringLiteral("longitude")).toDouble()));
        const double distance = station.value(QStringLiteral("distance_km")).toDouble();
        const double price = station.value(QStringLiteral("price_per_kwh")).toDouble();
        if (distance < minDistance_ || (maxDistance_ >= 0 && distance > maxDistance_)
            || price < minPrice_ || (maxPrice_ >= 0 && price > maxPrice_)) continue;
        if (!pileType_.isEmpty()) {
            bool match = false;
            for (const QVariant& item : stationPiles_.value(station.value(QStringLiteral("id")).toLongLong())) {
                const auto pile = item.toMap();
                if (pile.value(QStringLiteral("type")).toString() == pileType_
                    && (!idleOnly_ || pile.value(QStringLiteral("status")).toString() == QStringLiteral("idle"))) match = true;
            }
            if (!match) continue;
        } else if (idleOnly_ && station.value(QStringLiteral("idle_pile_count")).toInt() <= 0) continue;
        filtered.append(station);
    }
    std::sort(filtered.begin(), filtered.end(), [](const QVariant& left, const QVariant& right) {
        return left.toMap().value(QStringLiteral("distance_km")).toDouble()
            < right.toMap().value(QStringLiteral("distance_km")).toDouble();
    });
    stations_ = filtered;
    emit stationsChanged();
}

void UserAppController::updateUser(const QVariantMap& value)
{
    user_ = value;
    const QString avatar = user_.value(QStringLiteral("avatar_path")).toString();
    if (!avatar.isEmpty() && QImage(avatar).isNull()) user_.insert(QStringLiteral("avatar_path"), QString());
    emit userChanged();
}

void UserAppController::updateOrder(const QVariant& value)
{
    activeOrder_ = value.toMap();
    if (activeOrder_.value(QStringLiteral("status")).toString() == QStringLiteral("completed")
        || activeOrder_.value(QStringLiteral("status")).toString() == QStringLiteral("cancelled")) activeOrder_.clear();
    const QString status = activeOrder_.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("charging")) {
        const QDateTime started = QDateTime::fromString(
            activeOrder_.value(QStringLiteral("started_at")).toString(), Qt::ISODate);
        chargingSeconds_ = started.isValid()
            ? qMax<qint64>(0, started.secsTo(QDateTime::currentDateTime()))
            : 0;
        chargingTimer_.start();
    } else {
        chargingTimer_.stop();
        chargingSeconds_ = 0;
    }
    updateChargingEstimate();
    emit activeOrderChanged();
}

void UserAppController::updateChargingEstimate()
{
    const double energy = selectedPowerKw_ * chargingSeconds_ / 3600.0;
    chargingEstimate_ = QStringLiteral("%1:%2:%3 · %4 kWh · ￥%5")
        .arg(chargingSeconds_ / 3600, 2, 10, QLatin1Char('0'))
        .arg((chargingSeconds_ % 3600) / 60, 2, 10, QLatin1Char('0'))
        .arg(chargingSeconds_ % 60, 2, 10, QLatin1Char('0'))
        .arg(energy, 0, 'f', 3)
        .arg(energy * selectedPrice_, 0, 'f', 2);
    emit chargingEstimateChanged();
}

void UserAppController::handleResponse(const charging::core::Message& message)
{
    if (message.payload.value(QStringLiteral("code")).toString() == QStringLiteral("AUTH_USER_FROZEN")) {
        clearSession(); showNotice(QStringLiteral("账号已被冻结，请联系管理员"), QStringLiteral("error")); return;
    }
    if (!pending_.contains(message.id)) return; // Ignore responses from previous sessions.
    const QString requestType = pending_.take(message.id);
    if (pending_.isEmpty()) requestTimer_.stop();
    const bool mutation = requestType == QStringLiteral("auth.phone_login") || requestType == QStringLiteral("wallet.recharge")
        || requestType == QStringLiteral("user.profile.update") || requestType == QStringLiteral("order.reserve")
        || requestType == QStringLiteral("order.start") || requestType == QStringLiteral("order.cancel")
        || requestType == QStringLiteral("order.stop") || requestType == QStringLiteral("order.settle");
    if (mutation) setBusy(false);
    if (message.type.endsWith(QStringLiteral(".error"))) {
        const QString text =
            message.payload.value(QStringLiteral("message")).toString();
        showNotice(text.isEmpty() ? QStringLiteral("操作失败，请稍后重试") : text, QStringLiteral("error"));
        if (requestType == QStringLiteral("pile.list")) { loadingStation_ = 0; loadNextFilterPiles(); }
        if (requestType == QStringLiteral("order.settle") && (text.contains(QStringLiteral("余额"))
            || message.payload.value(QStringLiteral("code")).toString().contains(QStringLiteral("INSUFFICIENT")))) emit rechargeRequired();
        if (message.type == QStringLiteral("auth.phone_login.error"))
            emit authenticationRejected();
        if (message.payload.value(QStringLiteral("code")).toString()
            == QStringLiteral("ORDER_ACTIVE_EXISTS")) {
            sendRequest(QStringLiteral("order.active"));
        }
        return;
    }

    const QVariantMap payload = message.payload.toVariantMap();
    if (message.type == QStringLiteral("auth.phone_login.ok")) {
        updateUser(payload.value(QStringLiteral("user")).toMap());
        loggedIn_ = true;
        QSettings settings;
        settings.setValue(
            QStringLiteral("user/lastPhone"),
            user_.value(QStringLiteral("phone")).toString());
        emit loggedInChanged();
        emit loginSucceeded();
        refreshStations();
        refreshProfile();
        return;
    }
    if (message.type == QStringLiteral("station.list.ok")) {
        rawStations_ = payload.value(QStringLiteral("stations")).toList();
        stationPiles_.clear();
        filterQueue_.clear();
        for (const QVariant& value : rawStations_) {
            const qint64 id = value.toMap().value(QStringLiteral("id")).toLongLong();
            if (id != loadingStation_) filterQueue_.append(id);
        }
        loadNextFilterPiles();
        rebuildStations();
        return;
    }
    if (message.type == QStringLiteral("pile.list.ok")) {
        const auto receivedPiles = payload.value(QStringLiteral("piles")).toList();
        stationPiles_.insert(loadingStation_, receivedPiles);
        const bool selected = selectedStation_.value(QStringLiteral("id")).toLongLong() == loadingStation_;
        loadingStation_ = 0; loadNextFilterPiles(); rebuildStations();
        if (!selected) return;
        piles_ = receivedPiles;
        int offline = 0;
        for (const QVariant& pile : piles_)
            if (pile.toMap().value(QStringLiteral("status")).toString()
                == QStringLiteral("offline")) ++offline;
        selectedStation_.insert(QStringLiteral("offline_count"), offline);
        emit pilesChanged();
        emit selectedStationChanged();
        return;
    }
    if (message.type == QStringLiteral("user.profile.ok")
        || message.type == QStringLiteral("user.profile.update.ok")
        || message.type == QStringLiteral("wallet.recharge.ok")) {

    updateUser(payload.value(QStringLiteral("user")).toMap());

    // 如果是充值成功，立即重新获取充值记录，
    // 这样“我的”页面可以实时显示最新充值记录
    if (message.type == QStringLiteral("wallet.recharge.ok")) {
        sendRequest(QStringLiteral("recharge.history"));
        emit rechargeSucceeded();
    }

    showNotice(
        message.type == QStringLiteral("wallet.recharge.ok")
            ? QStringLiteral("充值成功")
            : QStringLiteral("资料已更新"),
        QStringLiteral("success"));

    return;
}
    if (message.type == QStringLiteral("order.active.ok")) {
        updateOrder(payload.value(QStringLiteral("order")));
        return;
    }
    if (message.type == QStringLiteral("order.history.ok")) {
        history_.clear();
        for (const QVariant& item : payload.value(QStringLiteral("orders")).toList()) {
            const QString status = item.toMap().value(QStringLiteral("status")).toString();
            if (status == QStringLiteral("completed") || status == QStringLiteral("cancelled")) history_.append(item);
        }
        std::sort(history_.begin(), history_.end(), [](const QVariant& a, const QVariant& b) {
            const auto left = a.toMap(), right = b.toMap();
            const auto lt = QDateTime::fromString(left.value(QStringLiteral("created_at")).toString(), Qt::ISODate);
            const auto rt = QDateTime::fromString(right.value(QStringLiteral("created_at")).toString(), Qt::ISODate);
            return lt == rt ? left.value(QStringLiteral("id")).toLongLong() > right.value(QStringLiteral("id")).toLongLong() : lt > rt;
        });
        emit historyChanged();
        return;
    }
    if (message.type == QStringLiteral("recharge.history.ok")) {
    rechargeHistory_ = payload.value(QStringLiteral("records")).toList();
    emit rechargeHistoryChanged();
    return;
}
    if (message.type.startsWith(QStringLiteral("order."))) {
        if (payload.contains(QStringLiteral("user")))
            updateUser(payload.value(QStringLiteral("user")).toMap());
        const QVariantMap order = payload.value(QStringLiteral("order")).toMap();
        const QString status = order.value(QStringLiteral("status")).toString();
        updateOrder(
            status == QStringLiteral("completed")
                    || status == QStringLiteral("cancelled")
                ? QVariant()
                : QVariant(order));
        if (message.type == QStringLiteral("order.reserve.ok"))
            emit reservationSucceeded();
        showNotice(QStringLiteral("订单状态已更新"), QStringLiteral("success"));
        sendRequest(QStringLiteral("order.history"));
        refreshStations();
        if (!selectedStation_.isEmpty())
            loadPiles(selectedStation_.value(QStringLiteral("id")).toLongLong());
    }
}

} // namespace charging::user
