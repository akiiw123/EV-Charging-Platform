#include "charging/core/request_router.h"

#include "charging/core/repositories.h"

#include <QJsonArray>
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

Message RequestRouter::route(const Message& request) const
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
        return success(request, {{QStringLiteral("user"), userJson(*user)}});
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
