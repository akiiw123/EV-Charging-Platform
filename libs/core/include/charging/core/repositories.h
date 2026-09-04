#pragma once

#include "charging/core/models.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <optional>

namespace charging::core {

class UserRepository final {
public:
    explicit UserRepository(QSqlDatabase database);

    std::optional<User> findByPhone(const QString& phone, QString* errorMessage = nullptr) const;
    std::optional<User> findById(qint64 id, QString* errorMessage = nullptr) const;
    std::optional<User> loginOrCreate(const QString& phone, QString* errorMessage = nullptr) const;
    bool updateNickname(qint64 userId, const QString& nickname, QString* errorMessage = nullptr) const;
    bool updateAvatarPath(qint64 userId, const QString& avatarPath,
                          QString* errorMessage = nullptr) const;
    bool recharge(qint64 userId, double amount, QString* errorMessage = nullptr) const;
    bool setStatus(qint64 userId, const QString& status, QString* errorMessage = nullptr) const;

private:
    mutable QSqlDatabase database_;
};

class AdministratorRepository final {
public:
    explicit AdministratorRepository(QSqlDatabase database);
    std::optional<Administrator> findByUsername(const QString& username,
                                                QString* errorMessage = nullptr) const;
    std::optional<Administrator> findById(qint64 administratorId,
                                          QString* errorMessage = nullptr) const;
    // 更新密码哈希并清除 must_change_password 标志(首登强制改密流程的收尾)
    bool changePassword(qint64 administratorId, const QString& passwordHash,
                        QString* errorMessage = nullptr) const;
    bool updatePasswordHash(qint64 administratorId, const QString& passwordHash,
                            QString* errorMessage = nullptr) const;

private:
    mutable QSqlDatabase database_;
};

class StationRepository final {
public:
    explicit StationRepository(QSqlDatabase database);

    QList<ChargingStation> list(QString* errorMessage = nullptr) const;
    std::optional<ChargingStation> findById(qint64 id, QString* errorMessage = nullptr) const;
    std::optional<ChargingStation> create(const ChargingStation& station,
                                          QString* errorMessage = nullptr) const;

private:
    mutable QSqlDatabase database_;
};

class PileRepository final {
public:
    explicit PileRepository(QSqlDatabase database);

    QList<ChargingPile> listByStation(qint64 stationId, QString* errorMessage = nullptr) const;
    std::optional<ChargingPile> findById(qint64 id, QString* errorMessage = nullptr) const;
    std::optional<ChargingPile> create(const ChargingPile& pile, QString* errorMessage = nullptr) const;
    // 编辑电桩类型与功率(管理端"编辑电桩"用;充电中应拒绝以避免计费口径变化)
    bool update(qint64 pileId, const QString& type, double powerKw,
                QString* errorMessage = nullptr) const;
    bool updateStatus(qint64 pileId, const QString& status, QString* errorMessage = nullptr) const;

private:
    mutable QSqlDatabase database_;
};

class OrderRepository final {
public:
    explicit OrderRepository(QSqlDatabase database);

    std::optional<ChargingOrder> findById(qint64 id, QString* errorMessage = nullptr) const;
    std::optional<ChargingOrder> findActiveByUser(qint64 userId,
                                                  QString* errorMessage = nullptr) const;
    QList<ChargingOrder> listByUser(qint64 userId, int limit = 50,
                                    QString* errorMessage = nullptr) const;
    std::optional<ChargingOrder> createReservation(qint64 userId, qint64 pileId,
                                                   QString* errorMessage = nullptr) const;
    bool startCharging(qint64 orderId, QString* errorMessage = nullptr) const;
    bool finishCharging(qint64 orderId, double energyKwh, double amount,
                        QString* errorMessage = nullptr) const;
    bool settle(qint64 orderId, QString* errorMessage = nullptr) const;
    bool cancel(qint64 orderId, QString* errorMessage = nullptr) const;

private:
    mutable QSqlDatabase database_;
};

class PricingRepository final {
public:
    explicit PricingRepository(QSqlDatabase database);

    // 站点未配置规则时返回 std::nullopt，调用方需回退到 charging_stations.price_per_kwh
    std::optional<PricingRule> findRule(qint64 stationId, QString* errorMessage = nullptr) const;
    QList<PricingPeriod> listPeriods(qint64 stationId, QString* errorMessage = nullptr) const;

    // 解析某时刻适用的电价：规则启用且命中时段则用分时电价，否则用站点固定电价。
    // 只有站点本身不存在时才失败。
    std::optional<double> pricePerKwhAt(qint64 stationId, const QDateTime& when,
                                        QString* errorMessage = nullptr) const;

    // 占位费 = max(0, occupiedMinutes - freeMoveMinutes) * occupancyFeePerMinute，
    // 并按 occupancyFeeCap 封顶（cap <= 0 表示不封顶），结果保留两位小数。
    std::optional<double> occupancyFee(qint64 stationId, int occupiedMinutes,
                                       QString* errorMessage = nullptr) const;

    bool saveRule(const PricingRule& rule, QString* errorMessage = nullptr) const;
    // 事务内整组替换该站的分时电价段；时段重叠或越界时整体失败，不留下半截数据
    bool replacePeriods(qint64 stationId, const QList<PricingPeriod>& periods,
                        QString* errorMessage = nullptr) const;

private:
    mutable QSqlDatabase database_;
};

} // namespace charging::core
