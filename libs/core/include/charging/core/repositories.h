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

} // namespace charging::core
