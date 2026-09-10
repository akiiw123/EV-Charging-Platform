#include "charging/core/database_manager.h"
#include "charging/core/repositories.h"

#include <QDate>
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTime>
#include <QUuid>
#include <QtTest>

// 覆盖小学期任务「收费数据：保存电价和占位规则」。
// 重点验证分时电价按时刻解析、免费挪车时间与占位费封顶，
// 以及新增表之后原有固定电价计费流程不受影响。
class PricingTest final : public QObject {
    Q_OBJECT

private slots:
    void seedProvidesTimeOfUsePeriodsAndRule()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::PricingRepository pricing(database);
            QString error;
            const auto rule = pricing.findRule(1, &error);
            QVERIFY2(rule.has_value(), qPrintable(error));
            QVERIFY(rule->enabled);
            QCOMPARE(rule->freeMoveMinutes, 15);
            QCOMPARE(rule->occupancyFeePerMinute, 0.5);
            QCOMPARE(rule->occupancyFeeCap, 30.0);

            const auto periods = pricing.listPeriods(1, &error);
            QCOMPARE(periods.size(), 5);
            // 时段按开始时间排序，且首尾覆盖全天
            QCOMPARE(periods.first().startMinute, 0);
            QCOMPARE(periods.last().endMinute, 1440);
            for (int index = 1; index < periods.size(); ++index) {
                QCOMPARE(periods[index].startMinute, periods[index - 1].endMinute);
            }
        });
    }

    void priceResolvesByTimeOfDay()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::PricingRepository pricing(database);
            QString error;
            // 谷 / 峰 / 平 / 峰 / 谷
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(3, 0), &error).value_or(-1), 0.80);
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(9, 0), &error).value_or(-1), 1.50);
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(12, 0), &error).value_or(-1), 1.20);
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(20, 0), &error).value_or(-1), 1.50);
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(23, 30), &error).value_or(-1), 0.80);
            QVERIFY2(error.isEmpty(), qPrintable(error));
        });
    }

    void periodBoundariesAreHalfOpen()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::PricingRepository pricing(database);
            // 08:00 属于峰段（左闭），07:59 仍属谷段（右开）
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(7, 59)).value_or(-1), 0.80);
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(8, 0)).value_or(-1), 1.50);
            // 22:00 进入谷段
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(21, 59)).value_or(-1), 1.50);
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(22, 0)).value_or(-1), 0.80);
        });
    }

    void priceFallsBackToFixedPriceWithoutPeriods()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::PricingRepository pricing(database);
            charging::core::StationRepository stations(database);
            // 900001 号站有占位费规则但没有分时电价段
            QVERIFY(pricing.listPeriods(900001).isEmpty());
            const auto station = stations.findById(900001);
            QVERIFY(station.has_value());
            QCOMPARE(pricing.pricePerKwhAt(900001, localTime(9, 0)).value_or(-1),
                     station->pricePerKwh);
        });
    }

    void disabledRuleFallsBackToFixedPrice()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::PricingRepository pricing(database);
            QString error;
            auto rule = pricing.findRule(1, &error).value();
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(9, 0), &error).value_or(-1), 1.50);

            rule.enabled = false;
            QVERIFY2(pricing.saveRule(rule, &error), qPrintable(error));
            // 关闭分时电价后回到站点固定电价 1.20
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(9, 0), &error).value_or(-1), 1.20);
            QCOMPARE(pricing.findRule(1, &error)->enabled, false);
        });
    }

    void priceForUnknownStationFails()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::PricingRepository pricing(database);
            QString error;
            QVERIFY(!pricing.pricePerKwhAt(999999, localTime(9, 0), &error).has_value());
            QVERIFY(error.contains(QStringLiteral("充电站不存在")));
            error.clear();
            QVERIFY(!pricing.pricePerKwhAt(1, QDateTime(), &error).has_value());
            QVERIFY(error.contains(QStringLiteral("时间点无效")));
        });
    }

    void occupancyFeeHonoursFreeMinutesAndCap()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::PricingRepository pricing(database);
            QString error;
            // 站点 1：免费挪车 15 分钟，每分钟 0.5 元，封顶 30 元
            QCOMPARE(pricing.occupancyFee(1, 0, &error).value_or(-1), 0.0);
            QCOMPARE(pricing.occupancyFee(1, 10, &error).value_or(-1), 0.0);
            QCOMPARE(pricing.occupancyFee(1, 15, &error).value_or(-1), 0.0);
            QCOMPARE(pricing.occupancyFee(1, 25, &error).value_or(-1), 5.0);
            QCOMPARE(pricing.occupancyFee(1, 60, &error).value_or(-1), 22.5);
            // 超出封顶后不再增长
            QCOMPARE(pricing.occupancyFee(1, 500, &error).value_or(-1), 30.0);
            QCOMPARE(pricing.occupancyFee(1, 100000, &error).value_or(-1), 30.0);
            QVERIFY2(error.isEmpty(), qPrintable(error));

            error.clear();
            QVERIFY(!pricing.occupancyFee(1, -1, &error).has_value());
            QVERIFY(error.contains(QStringLiteral("不能为负")));
        });
    }

    void occupancyFeeIsUncappedWhenCapIsZero()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::PricingRepository pricing(database);
            // 900001 号站：免费 10 分钟，每分钟 0.8 元，上限 0 表示不封顶
            QCOMPARE(pricing.occupancyFee(900001, 10).value_or(-1), 0.0);
            QCOMPARE(pricing.occupancyFee(900001, 100).value_or(-1), 72.0);
        });
    }

    void stationWithoutRulePaysNoOccupancyFee()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::StationRepository stations(database);
            charging::core::PricingRepository pricing(database);
            QString error;
            const auto station = createStation(stations, QStringLiteral("无规则站"), 2.0, &error);
            QVERIFY(station.has_value());
            QVERIFY(!pricing.findRule(station->id, &error).has_value());
            QCOMPARE(pricing.occupancyFee(station->id, 120, &error).value_or(-1), 0.0);
            QVERIFY2(error.isEmpty(), qPrintable(error));
            // 没有分时段时仍能用固定电价
            QCOMPARE(pricing.pricePerKwhAt(station->id, localTime(9, 0), &error).value_or(-1), 2.0);
        });
    }

    void saveRuleUpsertsSingleRow()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::StationRepository stations(database);
            charging::core::PricingRepository pricing(database);
            QString error;
            const auto station = createStation(stations, QStringLiteral("规则站"), 1.0, &error);
            QVERIFY(station.has_value());

            charging::core::PricingRule rule;
            rule.stationId = station->id;
            rule.enabled = true;
            rule.freeMoveMinutes = 5;
            rule.occupancyFeePerMinute = 0.2;
            rule.occupancyFeeCap = 10.0;
            QVERIFY2(pricing.saveRule(rule, &error), qPrintable(error));

            rule.freeMoveMinutes = 30;
            rule.occupancyFeePerMinute = 1.5;
            rule.occupancyFeeCap = 0;
            QVERIFY2(pricing.saveRule(rule, &error), qPrintable(error));

            const auto saved = pricing.findRule(station->id, &error);
            QVERIFY(saved.has_value());
            QCOMPARE(saved->freeMoveMinutes, 30);
            QCOMPARE(saved->occupancyFeePerMinute, 1.5);
            QCOMPARE(saved->occupancyFeeCap, 0.0);

            QSqlQuery count(database);
            QVERIFY(count.exec(QStringLiteral(
                "SELECT COUNT(*) FROM charging_pricing_rules WHERE station_id = %1")
                                   .arg(station->id)));
            QVERIFY(count.next());
            QCOMPARE(count.value(0).toInt(), 1);
        });
    }

    void saveRuleRejectsInvalidInput()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::PricingRepository pricing(database);
            QString error;

            charging::core::PricingRule rule;
            rule.stationId = 999999;
            QVERIFY(!pricing.saveRule(rule, &error));
            QVERIFY(error.contains(QStringLiteral("充电站不存在")));

            rule.stationId = 0;
            error.clear();
            QVERIFY(!pricing.saveRule(rule, &error));
            QVERIFY(error.contains(QStringLiteral("station_id 无效")));

            rule.stationId = 1;
            rule.freeMoveMinutes = -5;
            error.clear();
            QVERIFY(!pricing.saveRule(rule, &error));
            QVERIFY(error.contains(QStringLiteral("免费挪车时间")));

            rule.freeMoveMinutes = 0;
            rule.occupancyFeePerMinute = -0.1;
            error.clear();
            QVERIFY(!pricing.saveRule(rule, &error));
            QVERIFY(error.contains(QStringLiteral("每分钟占位费")));

            rule.occupancyFeePerMinute = 0.5;
            rule.occupancyFeeCap = -1.0;
            error.clear();
            QVERIFY(!pricing.saveRule(rule, &error));
            QVERIFY(error.contains(QStringLiteral("上限")));
        });
    }

    void replacePeriodsSwapsWholeSet()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::PricingRepository pricing(database);
            QString error;
            QCOMPARE(pricing.listPeriods(1, &error).size(), 5);

            // 乱序传入，落库后应按开始时间有序
            QList<charging::core::PricingPeriod> periods;
            periods.append(makePeriod(720, 1440, QStringLiteral("valley"), 0.60));
            periods.append(makePeriod(0, 720, QStringLiteral("peak"), 1.80));
            QVERIFY2(pricing.replacePeriods(1, periods, &error), qPrintable(error));

            const auto saved = pricing.listPeriods(1, &error);
            QCOMPARE(saved.size(), 2);
            QCOMPARE(saved[0].startMinute, 0);
            QCOMPARE(saved[0].pricePerKwh, 1.80);
            QCOMPARE(saved[1].startMinute, 720);
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(9, 0), &error).value_or(-1), 1.80);
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(23, 0), &error).value_or(-1), 0.60);

            // 清空分时段后回到固定电价
            QVERIFY2(pricing.replacePeriods(1, {}, &error), qPrintable(error));
            QVERIFY(pricing.listPeriods(1, &error).isEmpty());
            QCOMPARE(pricing.pricePerKwhAt(1, localTime(9, 0), &error).value_or(-1), 1.20);
        });
    }

    void replacePeriodsRejectsOverlapAndKeepsOldData()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::PricingRepository pricing(database);
            QString error;
            const auto original = pricing.listPeriods(1, &error);
            QCOMPARE(original.size(), 5);

            QList<charging::core::PricingPeriod> overlapping;
            overlapping.append(makePeriod(0, 600, QStringLiteral("valley"), 0.5));
            overlapping.append(makePeriod(480, 1440, QStringLiteral("peak"), 1.5));
            QVERIFY(!pricing.replacePeriods(1, overlapping, &error));
            QVERIFY(error.contains(QStringLiteral("重叠")));

            // 整组替换失败必须回滚，不能留下删空的半截数据
            const auto afterFailure = pricing.listPeriods(1, &error);
            QCOMPARE(afterFailure.size(), original.size());
            for (int index = 0; index < afterFailure.size(); ++index) {
                QCOMPARE(afterFailure[index].startMinute, original[index].startMinute);
                QCOMPARE(afterFailure[index].pricePerKwh, original[index].pricePerKwh);
            }
        });
    }

    void replacePeriodsRejectsOutOfRangeAndBadType()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::PricingRepository pricing(database);
            QString error;

            QList<charging::core::PricingPeriod> reversed;
            reversed.append(makePeriod(600, 0, QStringLiteral("peak"), 1.0));
            QVERIFY(!pricing.replacePeriods(1, reversed, &error));
            QVERIFY(error.contains(QStringLiteral("1440")));

            error.clear();
            QList<charging::core::PricingPeriod> tooFar;
            tooFar.append(makePeriod(0, 1441, QStringLiteral("peak"), 1.0));
            QVERIFY(!pricing.replacePeriods(1, tooFar, &error));

            error.clear();
            QList<charging::core::PricingPeriod> badType;
            badType.append(makePeriod(0, 60, QStringLiteral("super-peak"), 1.0));
            QVERIFY(!pricing.replacePeriods(1, badType, &error));
            QVERIFY(error.contains(QStringLiteral("时段类型")));

            error.clear();
            QList<charging::core::PricingPeriod> negativePrice;
            negativePrice.append(makePeriod(0, 60, QStringLiteral("peak"), -1.0));
            QVERIFY(!pricing.replacePeriods(1, negativePrice, &error));
            QVERIFY(error.contains(QStringLiteral("不能为负")));

            error.clear();
            QVERIFY(!pricing.replacePeriods(999999, {}, &error));
            QVERIFY(error.contains(QStringLiteral("充电站不存在")));
        });
    }

    void deletingStationCascadesToPricingData()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::StationRepository stations(database);
            charging::core::PricingRepository pricing(database);
            QString error;
            const auto station = createStation(stations, QStringLiteral("级联站"), 1.0, &error);
            QVERIFY(station.has_value());

            charging::core::PricingRule rule;
            rule.stationId = station->id;
            rule.freeMoveMinutes = 10;
            rule.occupancyFeePerMinute = 0.5;
            QVERIFY(pricing.saveRule(rule, &error));
            QVERIFY(pricing.replacePeriods(
                station->id, {makePeriod(0, 1440, QStringLiteral("flat"), 1.1)}, &error));

            QSqlQuery remove(database);
            remove.prepare(QStringLiteral("DELETE FROM charging_stations WHERE id = :id"));
            remove.bindValue(QStringLiteral(":id"), station->id);
            QVERIFY2(remove.exec(), qPrintable(remove.lastError().text()));

            QVERIFY(!pricing.findRule(station->id, &error).has_value());
            QVERIFY(pricing.listPeriods(station->id, &error).isEmpty());
        });
    }

    // 回归护栏：新增收费表之后，既有充电流程仍按站点固定电价结算
    void existingChargingFlowStillBillsWithFixedPrice()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::UserRepository users(database);
            charging::core::StationRepository stations(database);
            charging::core::PileRepository piles(database);
            charging::core::OrderRepository orders(database);
            QString error;

            const auto station = stations.findById(1, &error);
            QVERIFY(station.has_value());
            QCOMPARE(station->pricePerKwh, 1.20);

            const auto user = users.loginOrCreate(QStringLiteral("13500135000"), &error);
            QVERIFY(user.has_value());
            QVERIFY(users.recharge(user->id, 100.0, &error));

            const auto pile = piles.listByStation(1, &error).first();
            const auto order = orders.createReservation(user->id, pile.id, &error);
            QVERIFY2(order.has_value(), qPrintable(error));
            QVERIFY(orders.startCharging(order->id, &error));

            // 与 request_router 中 order.stop 相同的口径：电量 × 站点固定电价
            const double energy = 10.0;
            const double amount = qRound64(energy * station->pricePerKwh * 100.0) / 100.0;
            QCOMPARE(amount, 12.0);
            QVERIFY(orders.finishCharging(order->id, energy, amount, &error));
            QVERIFY(orders.settle(order->id, &error));

            QCOMPARE(orders.findById(order->id, &error)->amount, 12.0);
            QCOMPARE(users.findById(user->id, &error)->walletBalance, 88.0);
        });
    }

private:
    static QDateTime localTime(int hour, int minute)
    {
        return {QDate(2026, 9, 4), QTime(hour, minute)};
    }

    static charging::core::PricingPeriod makePeriod(int startMinute, int endMinute,
                                                    const QString& type, double price)
    {
        charging::core::PricingPeriod period;
        period.startMinute = startMinute;
        period.endMinute = endMinute;
        period.periodType = type;
        period.pricePerKwh = price;
        return period;
    }

    static std::optional<charging::core::ChargingStation> createStation(
        const charging::core::StationRepository& stations, const QString& name, double price,
        QString* error)
    {
        charging::core::ChargingStation input;
        input.name = name;
        input.address = QStringLiteral("测试地址");
        input.latitude = 39.9;
        input.longitude = 116.4;
        input.pricePerKwh = price;
        return stations.create(input, error);
    }

    template<typename Function>
    static void withDatabase(Function function)
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        {
            charging::core::DatabaseManager manager(QStringLiteral("pricing-")
                                                    + QUuid::createUuid().toString());
            QString error;
            QVERIFY2(manager.open(directory.filePath(QStringLiteral("test.db")), &error),
                     qPrintable(error));
            QVERIFY2(manager.initialize(&error), qPrintable(error));
            function(manager.database());
        }
    }
};

QTEST_GUILESS_MAIN(PricingTest)
#include "test_pricing.moc"
