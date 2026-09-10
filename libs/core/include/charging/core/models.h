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
    QString status = QStringLiteral("active");   // active 正常营业 / disabled 逻辑停用
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

// 分时电价段，对应 charging_pricing_periods。
// 分钟区间左闭右开，取值 [0, 1440]。
struct PricingPeriod {
    qint64 id = 0;
    qint64 stationId = 0;
    int startMinute = 0;
    int endMinute = 0;
    QString periodType;   // peak / flat / valley
    double pricePerKwh = 0.0;
};

// 站点收费规则，对应 charging_pricing_rules。
// enabled 为 false 时电价回退到 charging_stations.price_per_kwh；
// occupancyFeeCap <= 0 表示占位费不封顶。
struct PricingRule {
    qint64 stationId = 0;
    bool enabled = true;
    int freeMoveMinutes = 0;
    double occupancyFeePerMinute = 0.0;
    double occupancyFeeCap = 0.0;
    QDateTime updatedAt;
};

} // namespace charging::core
