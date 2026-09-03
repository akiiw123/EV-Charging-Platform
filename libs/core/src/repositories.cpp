#include "charging/core/repositories.h"

#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
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
    if (amount <= 0.0) {
        setError(errorMessage, QStringLiteral("充值金额必须大于 0"));
        return false;
    }
    if (!database_.transaction()) {
        setError(errorMessage, database_.lastError().text());
        return false;
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE users SET wallet_balance = wallet_balance + :amount WHERE id = :id"));
    query.bindValue(QStringLiteral(":amount"), amount);
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        return rollback(database_, errorMessage,
                        query.lastError().isValid() ? query.lastError().text()
                                                    : QStringLiteral("用户不存在"));
    }
    if (!database_.commit()) {
        return rollback(database_, errorMessage, database_.lastError().text());
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

} // namespace charging::core
