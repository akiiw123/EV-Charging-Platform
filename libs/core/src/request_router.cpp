#include "charging/core/request_router.h"

#include "charging/core/repositories.h"
#include "charging/core/password_security.h"

#include <QJsonArray>
#include <QSqlError>
#include <QSqlQuery>
#include <QtMath>
#include <utility>

namespace charging::core {
namespace {

QJsonObject userJson(const User& user)
{
    return {{QStringLiteral("id"), user.id},
            {QStringLiteral("phone"), user.phone},
            {QStringLiteral("nickname"), user.nickname},
            {QStringLiteral("avatar_path"), user.avatarPath},
            {QStringLiteral("wallet_balance"), user.walletBalance},
            {QStringLiteral("status"), user.status},
            {QStringLiteral("created_at"), user.createdAt.toString(Qt::ISODate)}};
}

QJsonObject pileJson(const ChargingPile& pile)
{
    return {{QStringLiteral("id"), pile.id},
            {QStringLiteral("station_id"), pile.stationId},
            {QStringLiteral("code"), pile.code},
            {QStringLiteral("type"), pile.type},
            {QStringLiteral("power_kw"), pile.powerKw},
            {QStringLiteral("status"), pile.status},
            {QStringLiteral("charge_count"), pile.chargeCount},
            {QStringLiteral("total_charge_minutes"), pile.totalChargeMinutes}};
}

QJsonObject stationJson(const ChargingStation& station, const QList<ChargingPile>& piles)
{
    int idleCount = 0;
    int offlineCount = 0;
    for (const auto& pile : piles) {
        idleCount += pile.status == QStringLiteral("idle") ? 1 : 0;
        offlineCount += pile.status == QStringLiteral("offline") ? 1 : 0;
    }
    return {{QStringLiteral("id"), station.id},
            {QStringLiteral("name"), station.name},
            {QStringLiteral("address"), station.address},
            {QStringLiteral("latitude"), station.latitude},
            {QStringLiteral("longitude"), station.longitude},
            {QStringLiteral("price_per_kwh"), station.pricePerKwh},
            {QStringLiteral("pile_count"), piles.size()},
            {QStringLiteral("idle_pile_count"), idleCount},
            {QStringLiteral("offline_count"), offlineCount},
            {QStringLiteral("online_rate"), piles.isEmpty() ? 0.0 : (piles.size() - offlineCount) * 100.0 / piles.size()},
            {QStringLiteral("created_at"), station.createdAt.toString(Qt::ISODate)}};
}

QJsonObject orderJson(const ChargingOrder& order)
{
    return {{QStringLiteral("id"), order.id},
            {QStringLiteral("user_id"), order.userId},
            {QStringLiteral("pile_id"), order.pileId},
            {QStringLiteral("status"), order.status},
            {QStringLiteral("started_at"), order.startedAt.toString(Qt::ISODate)},
            {QStringLiteral("ended_at"), order.endedAt.toString(Qt::ISODate)},
            {QStringLiteral("energy_kwh"), order.energyKwh},
            {QStringLiteral("amount"), order.amount},
            {QStringLiteral("created_at"), order.createdAt.toString(Qt::ISODate)}};
}

std::optional<qint64> positiveId(const QJsonObject& payload, const QString& name)
{
    const QJsonValue value = payload.value(name);
    if (!value.isDouble() || value.toInteger() <= 0) {
        return std::nullopt;
    }
    return value.toInteger();
}

} // namespace

RequestRouter::RequestRouter(QSqlDatabase database) : database_(std::move(database)) {}

Message RequestRouter::route(const Message& request)
{
    QString repositoryError;
    if (request.type == QStringLiteral("auth.phone_login")) {
        const QString phone = request.payload.value(QStringLiteral("phone")).toString();
        UserRepository users(database_);
        const auto user = users.loginOrCreate(phone, &repositoryError);
        if (!user) {
            return error(request, QStringLiteral("AUTH_INVALID_PHONE"),
                         repositoryError.isEmpty() ? QStringLiteral("手机号登录失败") : repositoryError);
        }
        if (user->status == QStringLiteral("frozen")) {
            return error(request, QStringLiteral("AUTH_USER_FROZEN"), QStringLiteral("用户已被冻结"));
        }
        authenticatedUserId_ = user->id;
        return success(request, {{QStringLiteral("user"), userJson(*user)}});
    }

    if (request.type == QStringLiteral("admin.login")) {
        AdministratorRepository administrators(database_);
        const QString username = request.payload.value(QStringLiteral("username")).toString();
        const QString password = request.payload.value(QStringLiteral("password")).toString();
        const auto administrator = administrators.findByUsername(username, &repositoryError);
        if (!administrator || !password::verify(password, administrator->passwordHash)) {
            return error(request, QStringLiteral("ADMIN_LOGIN_FAILED"), QStringLiteral("管理员账号或密码错误"));
        }
        if (password::needsUpgrade(administrator->passwordHash)
            && !administrators.updatePasswordHash(administrator->id, password::hash(password),
                                                  &repositoryError)) {
            return error(request, QStringLiteral("DATABASE_ERROR"), repositoryError);
        }
        authenticatedAdminId_ = administrator->id;
        return success(request, {{QStringLiteral("administrator"),
                                  QJsonObject {{QStringLiteral("id"), administrator->id},
                                               {QStringLiteral("username"), administrator->username},
                                               {QStringLiteral("must_change_password"), administrator->mustChangePassword}}}});
    }

    if (request.type.startsWith(QStringLiteral("admin.")) && !authenticatedAdminId_) {
        return error(request, QStringLiteral("ADMIN_AUTH_REQUIRED"), QStringLiteral("请先登录管理后台"));
    }

    if (request.type == QStringLiteral("admin.dashboard")) {
        QJsonObject metrics;
        QSqlQuery revenue(database_);
        if (!revenue.exec(QStringLiteral(
                "SELECT COALESCE(SUM(CASE WHEN date(created_at)=date('now','localtime') THEN amount END),0),"
                "COALESCE(SUM(CASE WHEN strftime('%Y-%m',created_at)=strftime('%Y-%m','now','localtime') THEN amount END),0),"
                "COALESCE(SUM(amount),0) FROM charging_orders WHERE status='completed'"))
            || !revenue.next()) {
            return error(request, QStringLiteral("DATABASE_ERROR"), revenue.lastError().text());
        }
        metrics.insert(QStringLiteral("today_revenue"), revenue.value(0).toDouble());
        metrics.insert(QStringLiteral("month_revenue"), revenue.value(1).toDouble());
        metrics.insert(QStringLiteral("total_revenue"), revenue.value(2).toDouble());
        QSqlQuery summary(database_);
        if (!summary.exec(QStringLiteral(
                "SELECT "
                "(SELECT COUNT(*) FROM charging_orders WHERE date(created_at)=date('now','localtime')),"
                "(SELECT COUNT(*) FROM charging_orders),"
                "(SELECT COUNT(*) FROM users),"
                "(SELECT COUNT(*) FROM charging_stations)")) || !summary.next()) {
            return error(request, QStringLiteral("DATABASE_ERROR"), summary.lastError().text());
        }
        metrics.insert(QStringLiteral("today_orders"), summary.value(0).toInt());
        metrics.insert(QStringLiteral("total_orders"), summary.value(1).toInt());
        metrics.insert(QStringLiteral("registered_users"), summary.value(2).toInt());
        metrics.insert(QStringLiteral("station_count"), summary.value(3).toInt());
        QSqlQuery pileCounts(database_);
        if (!pileCounts.exec(QStringLiteral("SELECT status,COUNT(*) FROM charging_piles GROUP BY status"))) {
            return error(request, QStringLiteral("DATABASE_ERROR"), pileCounts.lastError().text());
        }
        QJsonObject statuses {{QStringLiteral("idle"), 0}, {QStringLiteral("charging"), 0},
                              {QStringLiteral("fault"), 0}, {QStringLiteral("offline"), 0}};
        while (pileCounts.next()) statuses.insert(pileCounts.value(0).toString(), pileCounts.value(1).toInt());
        QJsonArray trend;
        QSqlQuery trendQuery(database_);
        trendQuery.exec(QStringLiteral(
            "SELECT date(created_at),SUM(amount) FROM charging_orders WHERE status='completed' "
            "AND date(created_at)>=date('now','-29 days') GROUP BY date(created_at) ORDER BY date(created_at)"));
        while (trendQuery.next()) {
            trend.append(QJsonObject {{QStringLiteral("date"), trendQuery.value(0).toString()},
                                      {QStringLiteral("amount"), trendQuery.value(1).toDouble()}});
        }
        int totalPiles = 0;
        for (const auto& value : statuses) totalPiles += value.toInt();
        const int onlinePiles = totalPiles - statuses.value(QStringLiteral("offline")).toInt();
        metrics.insert(QStringLiteral("online_piles"), onlinePiles);
        metrics.insert(QStringLiteral("fault_piles"), statuses.value(QStringLiteral("fault")).toInt());
        metrics.insert(QStringLiteral("online_rate"), totalPiles > 0 ? onlinePiles * 100.0 / totalPiles : 0.0);
        QJsonArray stationEnergy;
        QSqlQuery energyQuery(database_);
        energyQuery.exec(QStringLiteral(
            "SELECT s.name,COALESCE(SUM(o.energy_kwh),0) FROM charging_stations s "
            "LEFT JOIN charging_piles p ON p.station_id=s.id "
            "LEFT JOIN charging_orders o ON o.pile_id=p.id GROUP BY s.id ORDER BY 2 DESC LIMIT 8"));
        while (energyQuery.next()) stationEnergy.append(QJsonObject{{QStringLiteral("name"),energyQuery.value(0).toString()},{QStringLiteral("energy"),energyQuery.value(1).toDouble()}});
        return success(request, {{QStringLiteral("metrics"), metrics},
                                 {QStringLiteral("pile_status"), statuses},
                                 {QStringLiteral("revenue_trend"), trend},
                                 {QStringLiteral("station_energy"), stationEnergy}});
    }

    if (request.type == QStringLiteral("admin.station.list")) {
        StationRepository stations(database_); PileRepository piles(database_); QJsonArray array;
        for (const auto& station : stations.list(&repositoryError))
            array.append(stationJson(station, piles.listByStation(station.id, &repositoryError)));
        return repositoryError.isEmpty() ? success(request, {{QStringLiteral("stations"), array}})
                                         : error(request, QStringLiteral("DATABASE_ERROR"), repositoryError);
    }

    if (request.type == QStringLiteral("admin.station.create")) {
        ChargingStation value;
        value.name = request.payload.value(QStringLiteral("name")).toString();
        value.address = request.payload.value(QStringLiteral("address")).toString();
        value.latitude = request.payload.value(QStringLiteral("latitude")).toDouble(999);
        value.longitude = request.payload.value(QStringLiteral("longitude")).toDouble(999);
        value.pricePerKwh = request.payload.value(QStringLiteral("price_per_kwh")).toDouble(-1);
        const int pileCount = request.payload.value(QStringLiteral("pile_count")).toInt(0);
        if (value.latitude < -90 || value.latitude > 90 || value.longitude < -180 || value.longitude > 180
            || pileCount < 1 || pileCount > 100) {
            return error(request, QStringLiteral("INVALID_ARGUMENT"), QStringLiteral("经纬度或电桩数量无效"));
        }
        if (!database_.transaction()) return error(request, QStringLiteral("DATABASE_ERROR"), database_.lastError().text());
        StationRepository stations(database_); PileRepository piles(database_);
        const auto station = stations.create(value, &repositoryError);
        if (!station) { database_.rollback(); return error(request, QStringLiteral("STATION_CREATE_FAILED"), repositoryError); }
        for (int index = 1; index <= pileCount; ++index) {
            ChargingPile pile; pile.stationId = station->id;
            pile.code = QStringLiteral("ST%1-P%2").arg(station->id).arg(index, 3, 10, QLatin1Char('0'));
            pile.type = QStringLiteral("fast"); pile.powerKw = 60; pile.status = QStringLiteral("idle");
            if (!piles.create(pile, &repositoryError)) {
                database_.rollback(); return error(request, QStringLiteral("STATION_CREATE_FAILED"), repositoryError);
            }
        }
        if (!database_.commit()) return error(request, QStringLiteral("DATABASE_ERROR"), database_.lastError().text());
        return success(request, {{QStringLiteral("station"), stationJson(*station, piles.listByStation(station->id))}});
    }

    if (request.type == QStringLiteral("admin.station.update")) {
        const auto stationId = positiveId(request.payload, QStringLiteral("id"));
        const QString name = request.payload.value(QStringLiteral("name")).toString().trimmed();
        const QString address = request.payload.value(QStringLiteral("address")).toString().trimmed();
        const double latitude = request.payload.value(QStringLiteral("latitude")).toDouble(999);
        const double longitude = request.payload.value(QStringLiteral("longitude")).toDouble(999);
        const double price = request.payload.value(QStringLiteral("price_per_kwh")).toDouble(-1);
        if (!stationId || name.isEmpty() || address.isEmpty() || latitude < -90 || latitude > 90
            || longitude < -180 || longitude > 180 || price < 0) {
            return error(request, QStringLiteral("INVALID_ARGUMENT"), QStringLiteral("电站表单内容无效"));
        }
        QSqlQuery query(database_);
        query.prepare(QStringLiteral("UPDATE charging_stations SET name=:name,address=:address,latitude=:lat,longitude=:lng,price_per_kwh=:price WHERE id=:id"));
        query.bindValue(QStringLiteral(":name"), name); query.bindValue(QStringLiteral(":address"), address);
        query.bindValue(QStringLiteral(":lat"), latitude); query.bindValue(QStringLiteral(":lng"), longitude);
        query.bindValue(QStringLiteral(":price"), price); query.bindValue(QStringLiteral(":id"), *stationId);
        if (!query.exec() || query.numRowsAffected() != 1) return error(request, QStringLiteral("STATION_UPDATE_FAILED"), query.lastError().isValid() ? query.lastError().text() : QStringLiteral("电站不存在"));
        return success(request, {{QStringLiteral("id"), *stationId}});
    }

    if (request.type == QStringLiteral("admin.station.delete")) {
        const auto stationId = positiveId(request.payload, QStringLiteral("station_id"));
        if (!stationId) return error(request, QStringLiteral("INVALID_ARGUMENT"), QStringLiteral("station_id 无效"));
        QSqlQuery active(database_); active.prepare(QStringLiteral("SELECT COUNT(*) FROM charging_orders o JOIN charging_piles p ON p.id=o.pile_id WHERE p.station_id=:id AND o.status IN ('reserved','charging','awaiting_payment')")); active.bindValue(QStringLiteral(":id"),*stationId);
        if (!active.exec() || !active.next()) return error(request, QStringLiteral("DATABASE_ERROR"),active.lastError().text());
        if (active.value(0).toInt()>0) return error(request,QStringLiteral("STATION_DELETE_FAILED"),QStringLiteral("电站存在进行中或待结算订单，不能删除"));
        QSqlQuery query(database_); query.prepare(QStringLiteral("DELETE FROM charging_stations WHERE id=:id"));query.bindValue(QStringLiteral(":id"),*stationId);
        if(!query.exec()||query.numRowsAffected()!=1)return error(request,QStringLiteral("STATION_DELETE_FAILED"),query.lastError().isValid()?query.lastError().text():QStringLiteral("电站不存在"));
        return success(request,{{QStringLiteral("id"),*stationId}});
    }

    if (request.type == QStringLiteral("admin.pile.list")) {
        QSqlQuery query(database_);
        query.prepare(QStringLiteral(
            "SELECT p.id,p.station_id,p.code,p.type,p.power_kw,p.status,p.charge_count,p.total_charge_minutes,s.name "
            "FROM charging_piles p JOIN charging_stations s ON s.id=p.station_id ORDER BY p.id"));
        if (!query.exec()) return error(request, QStringLiteral("DATABASE_ERROR"), query.lastError().text());
        QJsonArray array;
        while (query.next()) {
            array.append(QJsonObject {{QStringLiteral("id"), query.value(0).toLongLong()},
                {QStringLiteral("station_id"), query.value(1).toLongLong()}, {QStringLiteral("code"), query.value(2).toString()},
                {QStringLiteral("type"), query.value(3).toString()}, {QStringLiteral("power_kw"), query.value(4).toDouble()},
                {QStringLiteral("status"), query.value(5).toString()}, {QStringLiteral("charge_count"), query.value(6).toInt()},
                {QStringLiteral("total_charge_minutes"), query.value(7).toInt()}, {QStringLiteral("station_name"), query.value(8).toString()}});
        }
        return success(request, {{QStringLiteral("piles"), array}});
    }

    if (request.type == QStringLiteral("admin.pile.restart")) {
        const auto pileId = positiveId(request.payload, QStringLiteral("pile_id"));
        if (!pileId) return error(request, QStringLiteral("INVALID_ARGUMENT"), QStringLiteral("pile_id 无效"));
        PileRepository piles(database_); const auto pile = piles.findById(*pileId, &repositoryError);
        if (!pile || pile->status == QStringLiteral("charging"))
            return error(request, QStringLiteral("PILE_RESTART_FAILED"), QStringLiteral("充电中的电桩不能重启"));
        if (!piles.updateStatus(*pileId, QStringLiteral("idle"), &repositoryError))
            return error(request, QStringLiteral("PILE_RESTART_FAILED"), repositoryError);
        return success(request, {{QStringLiteral("pile"), pileJson(*piles.findById(*pileId))}});
    }

    if (request.type == QStringLiteral("admin.user.list")) {
        QSqlQuery query(database_);
        query.prepare(QStringLiteral("SELECT u.*,MAX(o.created_at) AS last_activity,COUNT(o.id) AS order_count,COALESCE(SUM(CASE WHEN o.status='completed' THEN o.amount ELSE 0 END),0) AS total_spent FROM users u LEFT JOIN charging_orders o ON o.user_id=u.id WHERE u.phone LIKE :phone GROUP BY u.id ORDER BY u.id DESC LIMIT 200"));
        query.bindValue(QStringLiteral(":phone"), QStringLiteral("%") + request.payload.value(QStringLiteral("phone")).toString() + QStringLiteral("%"));
        if (!query.exec()) return error(request, QStringLiteral("DATABASE_ERROR"), query.lastError().text());
        QJsonArray array;
        while (query.next()) array.append(QJsonObject {{QStringLiteral("id"), query.value("id").toLongLong()},
            {QStringLiteral("phone"), query.value("phone").toString()}, {QStringLiteral("nickname"), query.value("nickname").toString()},
            {QStringLiteral("wallet_balance"), query.value("wallet_balance").toDouble()}, {QStringLiteral("status"), query.value("status").toString()},
            {QStringLiteral("created_at"), query.value("created_at").toString()}, {QStringLiteral("last_activity"), query.value("last_activity").toString()},
            {QStringLiteral("order_count"), query.value("order_count").toInt()}, {QStringLiteral("total_spent"), query.value("total_spent").toDouble()}});
        return success(request, {{QStringLiteral("users"), array}});
    }

    if (request.type == QStringLiteral("admin.order.list")) {
        QSqlQuery query(database_);
        query.prepare(QStringLiteral(
            "SELECT o.id,u.phone,s.name,p.code,o.status,o.created_at,o.started_at,o.ended_at,o.energy_kwh,o.amount,"
            "CASE WHEN o.started_at IS NULL THEN 0 ELSE CAST((julianday(COALESCE(o.ended_at,'now'))-julianday(o.started_at))*86400 AS INTEGER) END "
            "FROM charging_orders o JOIN users u ON u.id=o.user_id JOIN charging_piles p ON p.id=o.pile_id "
            "JOIN charging_stations s ON s.id=p.station_id ORDER BY o.id DESC LIMIT 500"));
        if (!query.exec()) return error(request,QStringLiteral("DATABASE_ERROR"),query.lastError().text());
        QJsonArray array; while(query.next()) array.append(QJsonObject{{QStringLiteral("id"),query.value(0).toLongLong()},
            {QStringLiteral("order_no"),QStringLiteral("#%1").arg(query.value(0).toLongLong(),6,10,QLatin1Char('0'))},
            {QStringLiteral("phone"),query.value(1).toString()},{QStringLiteral("station_name"),query.value(2).toString()},
            {QStringLiteral("pile_code"),query.value(3).toString()},{QStringLiteral("status"),query.value(4).toString()},
            {QStringLiteral("created_at"),query.value(5).toString()},{QStringLiteral("started_at"),query.value(6).toString()},
            {QStringLiteral("ended_at"),query.value(7).toString()},{QStringLiteral("energy_kwh"),query.value(8).toDouble()},
            {QStringLiteral("amount"),query.value(9).toDouble()},{QStringLiteral("duration_seconds"),query.value(10).toInt()}});
        return success(request,{{QStringLiteral("orders"),array}});
    }

    if (request.type == QStringLiteral("admin.user.status")) {
        const auto userId = positiveId(request.payload, QStringLiteral("user_id"));
        const QString status = request.payload.value(QStringLiteral("status")).toString();
        UserRepository users(database_);
        if (!userId || !users.setStatus(*userId, status, &repositoryError))
            return error(request, QStringLiteral("USER_STATUS_FAILED"), repositoryError.isEmpty() ? QStringLiteral("用户或状态无效") : repositoryError);
        return success(request, {{QStringLiteral("user"), userJson(*users.findById(*userId))}});
    }

    const bool requiresAuthentication = request.type.startsWith(QStringLiteral("user."))
        || request.type.startsWith(QStringLiteral("wallet."))
        || request.type.startsWith(QStringLiteral("order."));
    if (requiresAuthentication && !authenticatedUserId_) {
        return error(request, QStringLiteral("AUTH_REQUIRED"), QStringLiteral("请先登录"));
    }

    if (request.type == QStringLiteral("user.profile")) {
        UserRepository users(database_);
        const auto user = users.findById(*authenticatedUserId_, &repositoryError);
        return user ? success(request, {{QStringLiteral("user"), userJson(*user)}})
                    : error(request, QStringLiteral("USER_NOT_FOUND"), QStringLiteral("用户不存在"));
    }

    if (request.type == QStringLiteral("user.profile.update")) {
        UserRepository users(database_);
        const bool hasNickname = request.payload.contains(QStringLiteral("nickname"));
        const bool hasAvatar = request.payload.contains(QStringLiteral("avatar_path"));
        if (!hasNickname && !hasAvatar) {
            return error(request, QStringLiteral("INVALID_ARGUMENT"), QStringLiteral("没有可更新的资料"));
        }
        if (hasNickname
            && !users.updateNickname(*authenticatedUserId_,
                                     request.payload.value(QStringLiteral("nickname")).toString(),
                                     &repositoryError)) {
            return error(request, QStringLiteral("PROFILE_UPDATE_FAILED"), repositoryError);
        }
        if (hasAvatar
            && !users.updateAvatarPath(*authenticatedUserId_,
                                       request.payload.value(QStringLiteral("avatar_path")).toString(),
                                       &repositoryError)) {
            return error(request, QStringLiteral("PROFILE_UPDATE_FAILED"), repositoryError);
        }
        const auto user = users.findById(*authenticatedUserId_, &repositoryError);
        return user ? success(request, {{QStringLiteral("user"), userJson(*user)}})
                    : error(request, QStringLiteral("DATABASE_ERROR"), repositoryError);
    }

    if (request.type == QStringLiteral("wallet.recharge")) {
        const QJsonValue amountValue = request.payload.value(QStringLiteral("amount"));
        const double amount = amountValue.toDouble(-1.0);
        if (!amountValue.isDouble() || amount <= 0.0 || amount > 100000.0) {
            return error(request, QStringLiteral("INVALID_ARGUMENT"),
                         QStringLiteral("充值金额必须在 0 到 100000 元之间"));
        }
        UserRepository users(database_);
        if (!users.recharge(*authenticatedUserId_, amount, &repositoryError)) {
            return error(request, QStringLiteral("RECHARGE_FAILED"), repositoryError);
        }
        return success(request, {{QStringLiteral("user"),
                                  userJson(*users.findById(*authenticatedUserId_))}});
    }

    if (request.type == QStringLiteral("order.active")) {
        OrderRepository orders(database_);
        const auto order = orders.findActiveByUser(*authenticatedUserId_, &repositoryError);
        return success(request, {{QStringLiteral("order"),
                                  order ? QJsonValue(orderJson(*order)) : QJsonValue(QJsonValue::Null)}});
    }

    if (request.type == QStringLiteral("order.history")) {
        OrderRepository orders(database_);
        QJsonArray array;
        for (const auto& order : orders.listByUser(*authenticatedUserId_, 50, &repositoryError)) {
            QJsonObject item = orderJson(order);
            QSqlQuery names(database_);
            names.prepare(QStringLiteral(
                "SELECT p.code,s.name FROM charging_piles p "
                "JOIN charging_stations s ON s.id=p.station_id WHERE p.id=:pile"));
            names.bindValue(QStringLiteral(":pile"), order.pileId);
            if (names.exec() && names.next()) {
                item.insert(QStringLiteral("pile_code"), names.value(0).toString());
                item.insert(QStringLiteral("station_name"), names.value(1).toString());
            }
            array.append(item);
        }
        if (!repositoryError.isEmpty()) {
            return error(request, QStringLiteral("DATABASE_ERROR"), repositoryError);
        }
        return success(request, {{QStringLiteral("orders"), array}});
    }

    if (request.type == QStringLiteral("order.reserve")) {
        const auto pileId = positiveId(request.payload, QStringLiteral("pile_id"));
        if (!pileId) {
            return error(request, QStringLiteral("INVALID_ARGUMENT"), QStringLiteral("pile_id 无效"));
        }
        OrderRepository orders(database_);
        if (orders.findActiveByUser(*authenticatedUserId_)) {
            return error(request, QStringLiteral("ORDER_ACTIVE_EXISTS"),
                         QStringLiteral("您有未完成的充电订单，请先处理"));
        }
        const auto order = orders.createReservation(*authenticatedUserId_, *pileId, &repositoryError);
        return order ? success(request, {{QStringLiteral("order"), orderJson(*order)}})
                     : error(request, QStringLiteral("RESERVATION_FAILED"), repositoryError);
    }

    if (request.type.startsWith(QStringLiteral("order."))) {
        const auto orderId = positiveId(request.payload, QStringLiteral("order_id"));
        if (!orderId) {
            return error(request, QStringLiteral("INVALID_ARGUMENT"), QStringLiteral("order_id 无效"));
        }
        OrderRepository orders(database_);
        const auto existing = orders.findById(*orderId, &repositoryError);
        if (!existing || existing->userId != *authenticatedUserId_) {
            return error(request, QStringLiteral("ORDER_NOT_FOUND"), QStringLiteral("订单不存在"));
        }

        if (request.type == QStringLiteral("order.start")) {
            if (!orders.startCharging(*orderId, &repositoryError)) {
                return error(request, QStringLiteral("ORDER_START_FAILED"), repositoryError);
            }
        } else if (request.type == QStringLiteral("order.stop")) {
            QSqlQuery calculation(database_);
            calculation.prepare(QStringLiteral(
                "SELECT p.power_kw, s.price_per_kwh, "
                "MAX(1, CAST((julianday('now')-julianday(o.started_at))*86400 AS INTEGER)) "
                "FROM charging_orders o JOIN charging_piles p ON p.id=o.pile_id "
                "JOIN charging_stations s ON s.id=p.station_id "
                "WHERE o.id=:id AND o.user_id=:user AND o.status='charging'"));
            calculation.bindValue(QStringLiteral(":id"), *orderId);
            calculation.bindValue(QStringLiteral(":user"), *authenticatedUserId_);
            if (!calculation.exec() || !calculation.next()) {
                return error(request, QStringLiteral("ORDER_STOP_FAILED"),
                             calculation.lastError().isValid() ? calculation.lastError().text()
                                                               : QStringLiteral("订单不处于充电状态"));
            }
            const double energy = qMax(0.01, calculation.value(0).toDouble()
                                                     * calculation.value(2).toLongLong() / 3600.0);
            const double roundedEnergy = qRound64(energy * 1000.0) / 1000.0;
            const double amount = qRound64(roundedEnergy * calculation.value(1).toDouble() * 100.0)
                / 100.0;
            if (!orders.finishCharging(*orderId, roundedEnergy, amount, &repositoryError)) {
                return error(request, QStringLiteral("ORDER_STOP_FAILED"), repositoryError);
            }
        } else if (request.type == QStringLiteral("order.settle")) {
            if (!orders.settle(*orderId, &repositoryError)) {
                return error(request, QStringLiteral("ORDER_SETTLE_FAILED"), repositoryError);
            }
        } else if (request.type == QStringLiteral("order.cancel")) {
            if (!orders.cancel(*orderId, &repositoryError)) {
                return error(request, QStringLiteral("ORDER_CANCEL_FAILED"), repositoryError);
            }
        } else {
            return error(request, QStringLiteral("UNKNOWN_REQUEST"), QStringLiteral("不支持的订单操作"));
        }

        const auto updated = orders.findById(*orderId, &repositoryError);
        QJsonObject payload {{QStringLiteral("order"), orderJson(*updated)}};
        if (request.type == QStringLiteral("order.settle")) {
            UserRepository users(database_);
            payload.insert(QStringLiteral("user"), userJson(*users.findById(*authenticatedUserId_)));
        }
        return success(request, payload);
    }

    if (request.type == QStringLiteral("station.list")) {
        StationRepository stations(database_);
        PileRepository piles(database_);
        QJsonArray array;
        for (const auto& station : stations.list(&repositoryError)) {
            array.append(stationJson(station, piles.listByStation(station.id, &repositoryError)));
        }
        if (!repositoryError.isEmpty()) {
            return error(request, QStringLiteral("DATABASE_ERROR"), repositoryError);
        }
        return success(request, {{QStringLiteral("stations"), array}});
    }

    if (request.type == QStringLiteral("station.detail")) {
        const auto stationId = positiveId(request.payload, QStringLiteral("station_id"));
        if (!stationId) {
            return error(request, QStringLiteral("INVALID_ARGUMENT"), QStringLiteral("station_id 无效"));
        }
        StationRepository stations(database_);
        PileRepository piles(database_);
        const auto station = stations.findById(*stationId, &repositoryError);
        if (!station) {
            return error(request, QStringLiteral("STATION_NOT_FOUND"),
                         repositoryError.isEmpty() ? QStringLiteral("充电站不存在") : repositoryError);
        }
        QJsonArray pileArray;
        const auto stationPiles = piles.listByStation(*stationId, &repositoryError);
        for (const auto& pile : stationPiles) {
            pileArray.append(pileJson(pile));
        }
        QJsonObject payload = stationJson(*station, stationPiles);
        payload.insert(QStringLiteral("piles"), pileArray);
        return success(request, {{QStringLiteral("station"), payload}});
    }

    if (request.type == QStringLiteral("pile.list")) {
        const auto stationId = positiveId(request.payload, QStringLiteral("station_id"));
        if (!stationId) {
            return error(request, QStringLiteral("INVALID_ARGUMENT"), QStringLiteral("station_id 无效"));
        }
        PileRepository piles(database_);
        QJsonArray array;
        for (const auto& pile : piles.listByStation(*stationId, &repositoryError)) {
            array.append(pileJson(pile));
        }
        if (!repositoryError.isEmpty()) {
            return error(request, QStringLiteral("DATABASE_ERROR"), repositoryError);
        }
        return success(request, {{QStringLiteral("piles"), array}});
    }

    return error(request, QStringLiteral("UNKNOWN_REQUEST"), QStringLiteral("不支持的请求类型"));
}

Message RequestRouter::success(const Message& request, const QJsonObject& payload) const
{
    return {request.id, request.type + QStringLiteral(".ok"), payload};
}

Message RequestRouter::error(const Message& request, const QString& code,
                             const QString& message) const
{
    return {request.id, request.type + QStringLiteral(".error"),
            {{QStringLiteral("code"), code}, {QStringLiteral("message"), message}}};
}

} // namespace charging::core
