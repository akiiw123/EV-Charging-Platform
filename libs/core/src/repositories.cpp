#include "charging/core/repositories.h"

#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTime>
#include <QVariant>
#include <algorithm>
#include <utility>

namespace charging::core {
namespace {

void setError(QString* target, const QString& message)
{
    if (target) {
        *target = message;
    }
}

void setQueryError(QString* target, const QSqlQuery& query)
{
    setError(target, query.lastError().text());
}

QDateTime dateTime(const QVariant& value)
{
    if (value.isNull()) {
        return {};
    }
    QDateTime parsed = QDateTime::fromString(value.toString(), Qt::ISODate);
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(value.toString(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    if (parsed.isValid()) {
        // SQLite CURRENT_TIMESTAMP is UTC but has no timezone suffix.
        parsed.setTimeSpec(Qt::UTC);
        return parsed.toLocalTime();
    }
    return {};
}

User readUser(const QSqlQuery& query)
{
    return {query.value(QStringLiteral("id")).toLongLong(),
            query.value(QStringLiteral("phone")).toString(),
            query.value(QStringLiteral("nickname")).toString(),
            query.value(QStringLiteral("avatar_path")).toString(),
            query.value(QStringLiteral("wallet_balance")).toDouble(),
            query.value(QStringLiteral("status")).toString(),
            dateTime(query.value(QStringLiteral("created_at")))};
}

Administrator readAdministrator(const QSqlQuery& query)
{
    return {query.value(QStringLiteral("id")).toLongLong(),
            query.value(QStringLiteral("username")).toString(),
            query.value(QStringLiteral("password_hash")).toString(),
            query.value(QStringLiteral("must_change_password")).toBool(),
            dateTime(query.value(QStringLiteral("created_at")))};
}

ChargingStation readStation(const QSqlQuery& query)
{
    return {query.value(QStringLiteral("id")).toLongLong(),
            query.value(QStringLiteral("name")).toString(),
            query.value(QStringLiteral("address")).toString(),
            query.value(QStringLiteral("latitude")).toDouble(),
            query.value(QStringLiteral("longitude")).toDouble(),
            query.value(QStringLiteral("price_per_kwh")).toDouble(),
            dateTime(query.value(QStringLiteral("created_at")))};
}

ChargingPile readPile(const QSqlQuery& query)
{
    return {query.value(QStringLiteral("id")).toLongLong(),
            query.value(QStringLiteral("station_id")).toLongLong(),
            query.value(QStringLiteral("code")).toString(),
            query.value(QStringLiteral("type")).toString(),
            query.value(QStringLiteral("power_kw")).toDouble(),
            query.value(QStringLiteral("status")).toString(),
            query.value(QStringLiteral("charge_count")).toInt(),
            query.value(QStringLiteral("total_charge_minutes")).toInt()};
}

ChargingOrder readOrder(const QSqlQuery& query)
{
    return {query.value(QStringLiteral("id")).toLongLong(),
            query.value(QStringLiteral("user_id")).toLongLong(),
            query.value(QStringLiteral("pile_id")).toLongLong(),
            query.value(QStringLiteral("status")).toString(),
            dateTime(query.value(QStringLiteral("started_at"))),
            dateTime(query.value(QStringLiteral("ended_at"))),
            query.value(QStringLiteral("energy_kwh")).toDouble(),
            query.value(QStringLiteral("amount")).toDouble(),
            dateTime(query.value(QStringLiteral("created_at")))};
}

PricingRule readPricingRule(const QSqlQuery& query)
{
    return {query.value(QStringLiteral("station_id")).toLongLong(),
            query.value(QStringLiteral("enabled")).toBool(),
            query.value(QStringLiteral("free_move_minutes")).toInt(),
            query.value(QStringLiteral("occupancy_fee_per_minute")).toDouble(),
            query.value(QStringLiteral("occupancy_fee_cap")).toDouble(),
            dateTime(query.value(QStringLiteral("updated_at")))};
}

PricingPeriod readPricingPeriod(const QSqlQuery& query)
{
    return {query.value(QStringLiteral("id")).toLongLong(),
            query.value(QStringLiteral("station_id")).toLongLong(),
            query.value(QStringLiteral("start_minute")).toInt(),
            query.value(QStringLiteral("end_minute")).toInt(),
            query.value(QStringLiteral("period_type")).toString(),
            query.value(QStringLiteral("price_per_kwh")).toDouble()};
}

bool rollback(QSqlDatabase database, QString* errorMessage, const QString& message)
{
    database.rollback();
    setError(errorMessage, message);
    return false;
}

bool isValidPhone(const QString& phone)
{
    static const QRegularExpression pattern(QStringLiteral("^1[3-9]\\d{9}$"));
    return pattern.match(phone).hasMatch();
}

} // namespace

UserRepository::UserRepository(QSqlDatabase database) : database_(std::move(database)) {}

std::optional<User> UserRepository::findByPhone(const QString& phone, QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM users WHERE phone = :phone"));
    query.bindValue(QStringLiteral(":phone"), phone);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return std::nullopt;
    }
    return query.next() ? std::optional<User>(readUser(query)) : std::nullopt;
}

std::optional<User> UserRepository::findById(qint64 id, QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM users WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return std::nullopt;
    }
    return query.next() ? std::optional<User>(readUser(query)) : std::nullopt;
}

std::optional<User> UserRepository::loginOrCreate(const QString& phone, QString* errorMessage) const
{
    if (!isValidPhone(phone)) {
        setError(errorMessage, QStringLiteral("手机号格式不正确"));
        return std::nullopt;
    }
    if (const auto existing = findByPhone(phone, errorMessage)) {
        return existing;
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("INSERT INTO users(phone, nickname) VALUES(:phone, :nickname)"));
    query.bindValue(QStringLiteral(":phone"), phone);
    query.bindValue(QStringLiteral(":nickname"), QStringLiteral("用户") + phone.right(4));
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return std::nullopt;
    }
    return findById(query.lastInsertId().toLongLong(), errorMessage);
}

bool UserRepository::updateNickname(qint64 userId, const QString& nickname,
                                    QString* errorMessage) const
{
    if (nickname.trimmed().isEmpty() || nickname.trimmed().size() > 30) {
        setError(errorMessage, QStringLiteral("昵称长度必须为 1 到 30 个字符"));
        return false;
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("UPDATE users SET nickname = :nickname WHERE id = :id"));
    query.bindValue(QStringLiteral(":nickname"), nickname.trimmed());
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return false;
    }
    return query.numRowsAffected() == 1;
}

bool UserRepository::updateAvatarPath(qint64 userId, const QString& avatarPath,
                                      QString* errorMessage) const
{
    if (avatarPath.size() > 500) {
        setError(errorMessage, QStringLiteral("头像路径过长"));
        return false;
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("UPDATE users SET avatar_path = :path WHERE id = :id"));
    query.bindValue(QStringLiteral(":path"), avatarPath.trimmed());
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return false;
    }
    return query.numRowsAffected() == 1;
}

bool UserRepository::recharge(qint64 userId, double amount, QString* errorMessage) const
{
    // 充值金额必须大于 0
    if (amount <= 0.0) {
        setError(errorMessage, QStringLiteral("充值金额必须大于 0"));
        return false;
    }

    // 开启数据库事务：
    // 查询原余额、修改余额、写入充值记录必须同时成功
    if (!database_.transaction()) {
        setError(errorMessage, database_.lastError().text());
        return false;
    }

    QSqlQuery query(database_);

    // 1. 查询充值前余额
    query.prepare(QStringLiteral(
        "SELECT wallet_balance FROM users WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), userId);

    if (!query.exec() || !query.next()) {
        return rollback(
            database_,
            errorMessage,
            query.lastError().isValid()
                ? query.lastError().text()
                : QStringLiteral("用户不存在"));
    }

    const double balanceBefore = query.value(0).toDouble();
    const double balanceAfter = balanceBefore + amount;

    // 2. 更新用户钱包余额
    query.prepare(QStringLiteral(
        "UPDATE users "
        "SET wallet_balance = :balance "
        "WHERE id = :id"));
    query.bindValue(QStringLiteral(":balance"), balanceAfter);
    query.bindValue(QStringLiteral(":id"), userId);

    if (!query.exec() || query.numRowsAffected() != 1) {
        return rollback(
            database_,
            errorMessage,
            query.lastError().isValid()
                ? query.lastError().text()
                : QStringLiteral("充值失败"));
    }

    // 3. 写入充值记录
    query.prepare(QStringLiteral(
        "INSERT INTO recharge_records "
        "(user_id, amount, balance_before, balance_after) "
        "VALUES (:user_id, :amount, :before, :after)"));

    query.bindValue(QStringLiteral(":user_id"), userId);
    query.bindValue(QStringLiteral(":amount"), amount);
    query.bindValue(QStringLiteral(":before"), balanceBefore);
    query.bindValue(QStringLiteral(":after"), balanceAfter);

    if (!query.exec()) {
        return rollback(
            database_,
            errorMessage,
            query.lastError().text());
    }

    // 4. 两项操作都成功后才提交事务
    if (!database_.commit()) {
        return rollback(
            database_,
            errorMessage,
            database_.lastError().text());
    }

    return true;
}

bool UserRepository::setStatus(qint64 userId, const QString& status, QString* errorMessage) const
{
    if (status != QStringLiteral("active") && status != QStringLiteral("frozen")) {
        setError(errorMessage, QStringLiteral("用户状态无效"));
        return false;
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("UPDATE users SET status = :status WHERE id = :id"));
    query.bindValue(QStringLiteral(":status"), status);
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return false;
    }
    return query.numRowsAffected() == 1;
}

AdministratorRepository::AdministratorRepository(QSqlDatabase database)
    : database_(std::move(database)) {}

std::optional<Administrator> AdministratorRepository::findByUsername(
    const QString& username, QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM administrators WHERE username = :username"));
    query.bindValue(QStringLiteral(":username"), username);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return std::nullopt;
    }
    return query.next() ? std::optional<Administrator>(readAdministrator(query)) : std::nullopt;
}

std::optional<Administrator> AdministratorRepository::findById(
    qint64 administratorId, QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM administrators WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), administratorId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return std::nullopt;
    }
    return query.next() ? std::optional<Administrator>(readAdministrator(query)) : std::nullopt;
}

bool AdministratorRepository::changePassword(qint64 administratorId,
                                             const QString& passwordHash,
                                             QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE administrators SET password_hash = :password_hash, must_change_password = 0 "
        "WHERE id = :id"));
    query.bindValue(QStringLiteral(":password_hash"), passwordHash);
    query.bindValue(QStringLiteral(":id"), administratorId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return false;
    }
    return query.numRowsAffected() == 1;
}

bool AdministratorRepository::updatePasswordHash(qint64 administratorId,
                                                  const QString& passwordHash,
                                                  QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE administrators SET password_hash = :password_hash WHERE id = :id"));
    query.bindValue(QStringLiteral(":password_hash"), passwordHash);
    query.bindValue(QStringLiteral(":id"), administratorId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return false;
    }
    return query.numRowsAffected() == 1;
}

StationRepository::StationRepository(QSqlDatabase database) : database_(std::move(database)) {}

QList<ChargingStation> StationRepository::list(QString* errorMessage) const
{
    QList<ChargingStation> stations;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("SELECT * FROM charging_stations ORDER BY id"))) {
        setQueryError(errorMessage, query);
        return stations;
    }
    while (query.next()) {
        stations.append(readStation(query));
    }
    return stations;
}

std::optional<ChargingStation> StationRepository::findById(qint64 id,
                                                            QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM charging_stations WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return std::nullopt;
    }
    return query.next() ? std::optional<ChargingStation>(readStation(query)) : std::nullopt;
}

std::optional<ChargingStation> StationRepository::create(const ChargingStation& station,
                                                          QString* errorMessage) const
{
    if (station.name.trimmed().isEmpty() || station.address.trimmed().isEmpty()
        || station.pricePerKwh < 0.0) {
        setError(errorMessage, QStringLiteral("电站信息不完整"));
        return std::nullopt;
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO charging_stations(name,address,latitude,longitude,price_per_kwh) "
        "VALUES(:name,:address,:latitude,:longitude,:price)"));
    query.bindValue(QStringLiteral(":name"), station.name.trimmed());
    query.bindValue(QStringLiteral(":address"), station.address.trimmed());
    query.bindValue(QStringLiteral(":latitude"), station.latitude);
    query.bindValue(QStringLiteral(":longitude"), station.longitude);
    query.bindValue(QStringLiteral(":price"), station.pricePerKwh);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return std::nullopt;
    }
    return findById(query.lastInsertId().toLongLong(), errorMessage);
}

PileRepository::PileRepository(QSqlDatabase database) : database_(std::move(database)) {}

QList<ChargingPile> PileRepository::listByStation(qint64 stationId, QString* errorMessage) const
{
    QList<ChargingPile> piles;
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM charging_piles WHERE station_id = :station ORDER BY id"));
    query.bindValue(QStringLiteral(":station"), stationId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return piles;
    }
    while (query.next()) {
        piles.append(readPile(query));
    }
    return piles;
}

std::optional<ChargingPile> PileRepository::findById(qint64 id, QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM charging_piles WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return std::nullopt;
    }
    return query.next() ? std::optional<ChargingPile>(readPile(query)) : std::nullopt;
}

std::optional<ChargingPile> PileRepository::create(const ChargingPile& pile,
                                                    QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO charging_piles(station_id,code,type,power_kw,status) "
        "VALUES(:station,:code,:type,:power,:status)"));
    query.bindValue(QStringLiteral(":station"), pile.stationId);
    query.bindValue(QStringLiteral(":code"), pile.code.trimmed());
    query.bindValue(QStringLiteral(":type"), pile.type);
    query.bindValue(QStringLiteral(":power"), pile.powerKw);
    query.bindValue(QStringLiteral(":status"), pile.status.isEmpty() ? QStringLiteral("idle") : pile.status);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return std::nullopt;
    }
    return findById(query.lastInsertId().toLongLong(), errorMessage);
}

bool PileRepository::update(qint64 pileId, const QString& type, double powerKw,
                            QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE charging_piles SET type = :type, power_kw = :power WHERE id = :id"));
    query.bindValue(QStringLiteral(":type"), type);
    query.bindValue(QStringLiteral(":power"), powerKw);
    query.bindValue(QStringLiteral(":id"), pileId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return false;
    }
    return query.numRowsAffected() == 1;
}

bool PileRepository::updateStatus(qint64 pileId, const QString& status,
                                  QString* errorMessage) const
{
    static const QStringList allowed {QStringLiteral("idle"), QStringLiteral("charging"),
                                      QStringLiteral("fault"), QStringLiteral("offline")};
    if (!allowed.contains(status)) {
        setError(errorMessage, QStringLiteral("电桩状态无效"));
        return false;
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("UPDATE charging_piles SET status = :status WHERE id = :id"));
    query.bindValue(QStringLiteral(":status"), status);
    query.bindValue(QStringLiteral(":id"), pileId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return false;
    }
    return query.numRowsAffected() == 1;
}

OrderRepository::OrderRepository(QSqlDatabase database) : database_(std::move(database)) {}

std::optional<ChargingOrder> OrderRepository::findById(qint64 id, QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM charging_orders WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return std::nullopt;
    }
    return query.next() ? std::optional<ChargingOrder>(readOrder(query)) : std::nullopt;
}

std::optional<ChargingOrder> OrderRepository::findActiveByUser(qint64 userId,
                                                                QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "SELECT * FROM charging_orders WHERE user_id = :user AND "
        "status IN ('reserved','charging','awaiting_payment') LIMIT 1"));
    query.bindValue(QStringLiteral(":user"), userId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return std::nullopt;
    }
    return query.next() ? std::optional<ChargingOrder>(readOrder(query)) : std::nullopt;
}

QList<ChargingOrder> OrderRepository::listByUser(qint64 userId, int limit,
                                                  QString* errorMessage) const
{
    QList<ChargingOrder> orders;
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "SELECT * FROM charging_orders WHERE user_id = :user "
        "ORDER BY created_at DESC, id DESC LIMIT :limit"));
    query.bindValue(QStringLiteral(":user"), userId);
    query.bindValue(QStringLiteral(":limit"), qBound(1, limit, 200));
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return orders;
    }
    while (query.next()) {
        orders.append(readOrder(query));
    }
    return orders;
}

std::optional<ChargingOrder> OrderRepository::createReservation(qint64 userId, qint64 pileId,
                                                                 QString* errorMessage) const
{
    if (!database_.transaction()) {
        setError(errorMessage, database_.lastError().text());
        return std::nullopt;
    }
    QSqlQuery check(database_);
    check.prepare(QStringLiteral("SELECT status FROM charging_piles WHERE id = :id"));
    check.bindValue(QStringLiteral(":id"), pileId);
    if (!check.exec() || !check.next() || check.value(0).toString() != QStringLiteral("idle")) {
        rollback(database_, errorMessage, QStringLiteral("电桩不存在或当前不可预约"));
        return std::nullopt;
    }
    QSqlQuery insert(database_);
    insert.prepare(QStringLiteral(
        "INSERT INTO charging_orders(user_id,pile_id,status) VALUES(:user,:pile,'reserved')"));
    insert.bindValue(QStringLiteral(":user"), userId);
    insert.bindValue(QStringLiteral(":pile"), pileId);
    if (!insert.exec()) {
        rollback(database_, errorMessage, insert.lastError().text());
        return std::nullopt;
    }
    const qint64 orderId = insert.lastInsertId().toLongLong();
    if (!database_.commit()) {
        rollback(database_, errorMessage, database_.lastError().text());
        return std::nullopt;
    }
    return findById(orderId, errorMessage);
}

bool OrderRepository::startCharging(qint64 orderId, QString* errorMessage) const
{
    if (!database_.transaction()) {
        setError(errorMessage, database_.lastError().text());
        return false;
    }
    QSqlQuery order(database_);
    order.prepare(QStringLiteral(
        "UPDATE charging_orders SET status='charging', started_at=CURRENT_TIMESTAMP "
        "WHERE id=:id AND status='reserved'"));
    order.bindValue(QStringLiteral(":id"), orderId);
    if (!order.exec() || order.numRowsAffected() != 1) {
        return rollback(database_, errorMessage, QStringLiteral("订单不处于可启动状态"));
    }
    QSqlQuery pile(database_);
    pile.prepare(QStringLiteral(
        "UPDATE charging_piles SET status='charging' WHERE id="
        "(SELECT pile_id FROM charging_orders WHERE id=:id) AND status='idle'"));
    pile.bindValue(QStringLiteral(":id"), orderId);
    if (!pile.exec() || pile.numRowsAffected() != 1) {
        return rollback(database_, errorMessage, QStringLiteral("电桩当前不可用"));
    }
    return database_.commit();
}

bool OrderRepository::finishCharging(qint64 orderId, double energyKwh, double amount,
                                     QString* errorMessage) const
{
    if (energyKwh < 0.0 || amount < 0.0 || !database_.transaction()) {
        setError(errorMessage, QStringLiteral("充电量、金额或事务状态无效"));
        return false;
    }
    QSqlQuery order(database_);
    order.prepare(QStringLiteral(
        "UPDATE charging_orders SET status='awaiting_payment', ended_at=CURRENT_TIMESTAMP, "
        "energy_kwh=:energy, amount=:amount WHERE id=:id AND status='charging'"));
    order.bindValue(QStringLiteral(":energy"), energyKwh);
    order.bindValue(QStringLiteral(":amount"), amount);
    order.bindValue(QStringLiteral(":id"), orderId);
    if (!order.exec() || order.numRowsAffected() != 1) {
        return rollback(database_, errorMessage, QStringLiteral("订单不处于充电状态"));
    }
    QSqlQuery pile(database_);
    pile.prepare(QStringLiteral(
        "UPDATE charging_piles SET status='idle', charge_count=charge_count+1, "
        "total_charge_minutes=total_charge_minutes + MAX(1, "
        "CAST((julianday(CURRENT_TIMESTAMP)-julianday((SELECT started_at FROM charging_orders "
        "WHERE id=:id)))*1440 AS INTEGER)) WHERE id="
        "(SELECT pile_id FROM charging_orders WHERE id=:id) AND status='charging'"));
    pile.bindValue(QStringLiteral(":id"), orderId);
    if (!pile.exec() || pile.numRowsAffected() != 1) {
        return rollback(database_, errorMessage, QStringLiteral("更新电桩统计失败"));
    }
    return database_.commit();
}

bool OrderRepository::settle(qint64 orderId, QString* errorMessage) const
{
    if (!database_.transaction()) {
        setError(errorMessage, database_.lastError().text());
        return false;
    }
    QSqlQuery debit(database_);
    debit.prepare(QStringLiteral(
        "UPDATE users SET wallet_balance=wallet_balance-(SELECT amount FROM charging_orders WHERE id=:id) "
        "WHERE id=(SELECT user_id FROM charging_orders WHERE id=:id AND status='awaiting_payment') "
        "AND wallet_balance >= (SELECT amount FROM charging_orders WHERE id=:id)"));
    debit.bindValue(QStringLiteral(":id"), orderId);
    if (!debit.exec() || debit.numRowsAffected() != 1) {
        return rollback(database_, errorMessage, QStringLiteral("余额不足或订单不可结算"));
    }
    QSqlQuery order(database_);
    order.prepare(QStringLiteral(
        "UPDATE charging_orders SET status='completed' WHERE id=:id AND status='awaiting_payment'"));
    order.bindValue(QStringLiteral(":id"), orderId);
    if (!order.exec() || order.numRowsAffected() != 1) {
        return rollback(database_, errorMessage, QStringLiteral("更新订单状态失败"));
    }
    return database_.commit();
}

bool OrderRepository::cancel(qint64 orderId, QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE charging_orders SET status='cancelled' WHERE id=:id AND status='reserved'"));
    query.bindValue(QStringLiteral(":id"), orderId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return false;
    }
    return query.numRowsAffected() == 1;
}

namespace {

// 区分"站点不存在"与"站点未配置收费规则"，避免把外键错误直接抛给上层
bool stationExists(const QSqlDatabase& database, qint64 stationId, QString* errorMessage)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT id FROM charging_stations WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), stationId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return false;
    }
    if (!query.next()) {
        setError(errorMessage, QStringLiteral("充电站不存在"));
        return false;
    }
    return true;
}

} // namespace

PricingRepository::PricingRepository(QSqlDatabase database) : database_(std::move(database)) {}

std::optional<PricingRule> PricingRepository::findRule(qint64 stationId, QString* errorMessage) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM charging_pricing_rules WHERE station_id = :station"));
    query.bindValue(QStringLiteral(":station"), stationId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return std::nullopt;
    }
    return query.next() ? std::optional<PricingRule>(readPricingRule(query)) : std::nullopt;
}

QList<PricingPeriod> PricingRepository::listPeriods(qint64 stationId, QString* errorMessage) const
{
    QList<PricingPeriod> periods;
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "SELECT * FROM charging_pricing_periods WHERE station_id = :station ORDER BY start_minute"));
    query.bindValue(QStringLiteral(":station"), stationId);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return periods;
    }
    while (query.next()) {
        periods.append(readPricingPeriod(query));
    }
    return periods;
}

std::optional<double> PricingRepository::pricePerKwhAt(qint64 stationId, const QDateTime& when,
                                                       QString* errorMessage) const
{
    if (!when.isValid()) {
        setError(errorMessage, QStringLiteral("时间点无效"));
        return std::nullopt;
    }

    QSqlQuery station(database_);
    station.prepare(QStringLiteral("SELECT price_per_kwh FROM charging_stations WHERE id = :id"));
    station.bindValue(QStringLiteral(":id"), stationId);
    if (!station.exec()) {
        setQueryError(errorMessage, station);
        return std::nullopt;
    }
    if (!station.next()) {
        setError(errorMessage, QStringLiteral("充电站不存在"));
        return std::nullopt;
    }
    const double fixedPrice = station.value(0).toDouble();

    const QTime local = when.toLocalTime().time();
    const int minuteOfDay = local.hour() * 60 + local.minute();

    QSqlQuery period(database_);
    period.prepare(QStringLiteral(
        "SELECT p.price_per_kwh FROM charging_pricing_periods p "
        "JOIN charging_pricing_rules r ON r.station_id = p.station_id "
        "WHERE p.station_id = :station AND r.enabled = 1 "
        "AND p.start_minute <= :minute AND p.end_minute > :minute LIMIT 1"));
    period.bindValue(QStringLiteral(":station"), stationId);
    period.bindValue(QStringLiteral(":minute"), minuteOfDay);
    if (!period.exec()) {
        setQueryError(errorMessage, period);
        return std::nullopt;
    }
    // 未启用分时电价或未覆盖该时刻时，沿用站点固定电价
    return period.next() ? std::optional<double>(period.value(0).toDouble())
                         : std::optional<double>(fixedPrice);
}

std::optional<double> PricingRepository::occupancyFee(qint64 stationId, int occupiedMinutes,
                                                      QString* errorMessage) const
{
    if (occupiedMinutes < 0) {
        setError(errorMessage, QStringLiteral("占位分钟数不能为负"));
        return std::nullopt;
    }
    if (!stationExists(database_, stationId, errorMessage)) {
        return std::nullopt;
    }

    QString ruleError;
    const auto rule = findRule(stationId, &ruleError);
    if (!rule) {
        if (!ruleError.isEmpty()) {
            setError(errorMessage, ruleError);
            return std::nullopt;
        }
        return 0.0;   // 未配置收费规则的站点不收取占位费
    }

    const int billableMinutes = qMax(0, occupiedMinutes - rule->freeMoveMinutes);
    double fee = billableMinutes * rule->occupancyFeePerMinute;
    if (rule->occupancyFeeCap > 0.0) {
        fee = qMin(fee, rule->occupancyFeeCap);
    }
    return qRound64(fee * 100.0) / 100.0;
}

bool PricingRepository::saveRule(const PricingRule& rule, QString* errorMessage) const
{
    if (rule.stationId <= 0) {
        setError(errorMessage, QStringLiteral("station_id 无效"));
        return false;
    }
    if (rule.freeMoveMinutes < 0) {
        setError(errorMessage, QStringLiteral("免费挪车时间不能为负"));
        return false;
    }
    if (rule.occupancyFeePerMinute < 0.0) {
        setError(errorMessage, QStringLiteral("每分钟占位费不能为负"));
        return false;
    }
    if (rule.occupancyFeeCap < 0.0) {
        setError(errorMessage, QStringLiteral("占位费上限不能为负"));
        return false;
    }
    if (!stationExists(database_, rule.stationId, errorMessage)) {
        return false;
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO charging_pricing_rules"
        "(station_id, enabled, free_move_minutes, occupancy_fee_per_minute, occupancy_fee_cap) "
        "VALUES(:station, :enabled, :free, :per_minute, :cap) "
        "ON CONFLICT(station_id) DO UPDATE SET "
        "enabled = excluded.enabled, "
        "free_move_minutes = excluded.free_move_minutes, "
        "occupancy_fee_per_minute = excluded.occupancy_fee_per_minute, "
        "occupancy_fee_cap = excluded.occupancy_fee_cap, "
        "updated_at = CURRENT_TIMESTAMP"));
    query.bindValue(QStringLiteral(":station"), rule.stationId);
    query.bindValue(QStringLiteral(":enabled"), rule.enabled ? 1 : 0);
    query.bindValue(QStringLiteral(":free"), rule.freeMoveMinutes);
    query.bindValue(QStringLiteral(":per_minute"), rule.occupancyFeePerMinute);
    query.bindValue(QStringLiteral(":cap"), rule.occupancyFeeCap);
    if (!query.exec()) {
        setQueryError(errorMessage, query);
        return false;
    }
    return true;
}

bool PricingRepository::replacePeriods(qint64 stationId, const QList<PricingPeriod>& periods,
                                       QString* errorMessage) const
{
    if (stationId <= 0) {
        setError(errorMessage, QStringLiteral("station_id 无效"));
        return false;
    }

    static const QStringList allowedTypes {QStringLiteral("peak"), QStringLiteral("flat"),
                                           QStringLiteral("valley")};
    QList<PricingPeriod> sorted = periods;
    for (const auto& period : sorted) {
        if (period.startMinute < 0 || period.endMinute > 1440
            || period.startMinute >= period.endMinute) {
            setError(errorMessage, QStringLiteral("时段区间必须在 0 到 1440 分钟内且开始早于结束"));
            return false;
        }
        if (!allowedTypes.contains(period.periodType)) {
            setError(errorMessage, QStringLiteral("时段类型无效"));
            return false;
        }
        if (period.pricePerKwh < 0.0) {
            setError(errorMessage, QStringLiteral("分时电价不能为负"));
            return false;
        }
    }

    std::sort(sorted.begin(), sorted.end(), [](const PricingPeriod& left, const PricingPeriod& right) {
        return left.startMinute < right.startMinute;
    });
    for (int index = 1; index < sorted.size(); ++index) {
        if (sorted[index].startMinute < sorted[index - 1].endMinute) {
            setError(errorMessage, QStringLiteral("分时电价时段存在重叠"));
            return false;
        }
    }

    if (!stationExists(database_, stationId, errorMessage)) {
        return false;
    }

    if (!database_.transaction()) {
        setError(errorMessage, database_.lastError().text());
        return false;
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral("DELETE FROM charging_pricing_periods WHERE station_id = :station"));
    query.bindValue(QStringLiteral(":station"), stationId);
    if (!query.exec()) {
        return rollback(database_, errorMessage, query.lastError().text());
    }

    for (const auto& period : sorted) {
        query.prepare(QStringLiteral(
            "INSERT INTO charging_pricing_periods"
            "(station_id, start_minute, end_minute, period_type, price_per_kwh) "
            "VALUES(:station, :start, :end, :type, :price)"));
        query.bindValue(QStringLiteral(":station"), stationId);
        query.bindValue(QStringLiteral(":start"), period.startMinute);
        query.bindValue(QStringLiteral(":end"), period.endMinute);
        query.bindValue(QStringLiteral(":type"), period.periodType);
        query.bindValue(QStringLiteral(":price"), period.pricePerKwh);
        if (!query.exec()) {
            return rollback(database_, errorMessage, query.lastError().text());
        }
    }

    if (!database_.commit()) {
        return rollback(database_, errorMessage, database_.lastError().text());
    }
    return true;
}

} // namespace charging::core
