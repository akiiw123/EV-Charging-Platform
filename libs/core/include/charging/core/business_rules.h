#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QTime>

#include "charging/core/models.h"

#include <QtGlobal>

// 充电业务规则常量与纯计算(全局唯一的取值来源,服务端所有相关判断引用此处;
// 纯函数不访问数据库,便于单元测试覆盖)

namespace charging::core {

// 预约保留时长(分钟):超时未开始充电将被系统自动取消并释放电桩。
// 历史订单中"用户取消"与"系统超时取消"统一存储为 cancelled,
// 面向用户的失败提示按创建时间是否超出本窗口来区分口径。
constexpr int kReservationTimeoutMinutes = 15;

// 分时电价逐段积分:从 startedAt 起 durationSeconds 内,逐秒归属到该秒所属的
// 电价时段(分钟区间左闭右开,跨天按当日分钟数循环),电量按"价格分桶"累计,
// 每桶电量四舍五入到 0.001 度后乘以该时段单价求和。
// periods 为空(站点未配置规则或 enabled=0 或时段未覆盖)时退化为单段固定电价,
// 与既有"round3(电量) × 固定单价"口径完全一致;结果未做金额取整,由调用方保留两位。
inline double integratedChargingCost(const QDateTime& startedAt, qint64 durationSeconds,
                                     double powerKw, double fixedPricePerKwh,
                                     const QList<PricingPeriod>& periods)
{
    if (durationSeconds <= 0 || powerKw <= 0.0) {
        return 0.0;
    }
    double priceByMinute[1440];
    for (int minute = 0; minute < 1440; ++minute) {
        priceByMinute[minute] = fixedPricePerKwh;
    }
    for (const auto& period : periods) {
        for (int minute = period.startMinute; minute < period.endMinute; ++minute) {
            priceByMinute[minute] = period.pricePerKwh;
        }
    }
    QHash<double, qint64> secondsByPrice;
    QDateTime cursor = startedAt;
    qint64 remaining = durationSeconds;
    while (remaining > 0) {
        const QTime time = cursor.toLocalTime().time();
        const int minuteOfDay = time.hour() * 60 + time.minute();
        const qint64 chunk = qMin(remaining, static_cast<qint64>(60 - time.second()));
        secondsByPrice[priceByMinute[minuteOfDay]] += chunk;
        remaining -= chunk;
        cursor = cursor.addSecs(static_cast<int>(chunk));
    }
    double cost = 0.0;
    for (auto it = secondsByPrice.constBegin(); it != secondsByPrice.constEnd(); ++it) {
        const double segmentEnergy =
            qRound64(powerKw * it.value() / 3600.0 * 1000.0) / 1000.0;
        cost += segmentEnergy * it.key();
    }
    return cost;
}

} // namespace charging::core
