#pragma once

#include <QDateTime>
#include <QString>

namespace charging::core {

struct User {
    qint64 id = 0;
    QString phone;
    QString nickname;
    QString avatarPath;
    double walletBalance = 0.0;
    QString status;
    QDateTime createdAt;
};

struct Administrator {
    qint64 id = 0;
    QString username;
    QString passwordHash;
    bool mustChangePassword = true;
    QDateTime createdAt;
};

struct ChargingStation {
    qint64 id = 0;
    QString name;
    QString address;
    double latitude = 0.0;
    double longitude = 0.0;
    double pricePerKwh = 0.0;
    QDateTime createdAt;
};

struct ChargingPile {
    qint64 id = 0;
    qint64 stationId = 0;
    QString code;
    QString type;
    double powerKw = 0.0;
    QString status;
    int chargeCount = 0;
    int totalChargeMinutes = 0;
};

struct ChargingOrder {
    qint64 id = 0;
    qint64 userId = 0;
    qint64 pileId = 0;
    QString status;
    QDateTime startedAt;
    QDateTime endedAt;
    double energyKwh = 0.0;
    double amount = 0.0;
    QDateTime createdAt;
};

} // namespace charging::core
