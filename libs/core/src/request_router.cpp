#include "charging/core/request_router.h"

#include "charging/core/repositories.h"

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
    for (const auto& pile : piles) {
        idleCount += pile.status == QStringLiteral("idle") ? 1 : 0;
    }
    return {{QStringLiteral("id"), station.id},
            {QStringLiteral("name"), station.name},
            {QStringLiteral("address"), station.address},
            {QStringLiteral("latitude"), station.latitude},
            {QStringLiteral("longitude"), station.longitude},
            {QStringLiteral("price_per_kwh"), station.pricePerKwh},
            {QStringLiteral("pile_count"), piles.size()},
            {QStringLiteral("idle_pile_count"), idleCount},
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
            array.append(orderJson(order));
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
