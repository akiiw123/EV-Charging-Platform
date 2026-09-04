#include "charging/core/database_maintenance.h"
#include "charging/core/database_manager.h"
#include "charging/core/repositories.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QSemaphore>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>
#include <QVariant>
#include <QVector>
#include <QtTest>
#include <utility>

// 覆盖小学期任务「数据库测试」中尚未覆盖的特殊情况：
// 错误数据、保存失败、同时操作、以及记录较多时的表现。
class DatabaseRobustnessTest final : public QObject {
    Q_OBJECT

private slots:
    // ---------- 错误数据 ----------

    void schemaRejectsInvalidFieldValues()
    {
        withDatabase([](QSqlDatabase database) {
            expectRejected(database,
                           "INSERT INTO users(phone, nickname) VALUES(:phone, :nick)",
                           {{":phone", "1380013"}, {":nick", "号码太短"}});
            expectRejected(database,
                           "INSERT INTO users(phone, nickname, wallet_balance) "
                           "VALUES(:phone, :nick, :balance)",
                           {{":phone", "13800138001"}, {":nick", "负余额"}, {":balance", -1.0}});
            expectRejected(database,
                           "INSERT INTO users(phone, nickname, status) VALUES(:phone, :nick, :status)",
                           {{":phone", "13800138002"}, {":nick", "非法状态"}, {":status", "banned"}});
            expectRejected(database,
                           "INSERT INTO charging_stations(name, address, latitude, longitude, "
                           "price_per_kwh) VALUES(:n, :a, 0, 0, :price)",
                           {{":n", "负电价站"}, {":a", "地址"}, {":price", -0.5}});
            expectRejected(database,
                           "INSERT INTO charging_piles(station_id, code, type, power_kw) "
                           "VALUES(1, :code, :type, 60)",
                           {{":code", "BAD-TYPE"}, {":type", "medium"}});
            expectRejected(database,
                           "INSERT INTO charging_piles(station_id, code, type, power_kw) "
                           "VALUES(1, :code, 'fast', :power)",
                           {{":code", "BAD-POWER"}, {":power", 0.0}});
            expectRejected(database,
                           "INSERT INTO charging_piles(station_id, code, type, power_kw, status) "
                           "VALUES(1, :code, 'fast', 60, :status)",
                           {{":code", "BAD-STATUS"}, {":status", "broken"}});
            expectRejected(database,
                           "INSERT INTO charging_orders(user_id, pile_id, status) "
                           "VALUES(900001, 1, :status)",
                           {{":status", "finished"}});
            expectRejected(database,
                           "INSERT INTO charging_orders(user_id, pile_id, status, energy_kwh) "
                           "VALUES(900001, 1, 'completed', :energy)",
                           {{":energy", -1.0}});
            expectRejected(database,
                           "INSERT INTO recharge_records(user_id, amount) VALUES(900001, :amount)",
                           {{":amount", 0.0}});
            expectRejected(database,
                           "INSERT INTO charging_pricing_periods(station_id, start_minute, "
                           "end_minute, period_type, price_per_kwh) "
                           "VALUES(1, :start, :end, 'peak', 1.0)",
                           {{":start", 600}, {":end", 600}});
            expectRejected(database,
                           "INSERT INTO charging_pricing_periods(station_id, start_minute, "
                           "end_minute, period_type, price_per_kwh) "
                           "VALUES(1, :start, :end, 'peak', 1.0)",
                           {{":start", 0}, {":end", 1441}});
        });
    }

    void schemaRejectsDuplicateUniqueValues()
    {
        withDatabase([](QSqlDatabase database) {
            expectRejected(database,
                           "INSERT INTO users(phone, nickname) VALUES(:phone, :nick)",
                           {{":phone", "18800000001"}, {":nick", "重复手机号"}});
            expectRejected(database,
                           "INSERT INTO administrators(username, password_hash) "
                           "VALUES(:name, :hash)",
                           {{":name", "admin"}, {":hash", "x"}});
            expectRejected(database,
                           "INSERT INTO charging_piles(station_id, code, type, power_kw) "
                           "VALUES(1, :code, 'fast', 60)",
                           {{":code", "PILE-001"}});
            expectRejected(database,
                           "INSERT INTO charging_pricing_periods(station_id, start_minute, "
                           "end_minute, period_type, price_per_kwh) "
                           "VALUES(1, :start, 600, 'peak', 1.0)",
                           {{":start", 0}});

            // 唯一索引只约束活动订单：同一用户的历史订单可以有任意多条
            QSqlQuery history(database);
            QVERIFY(history.prepare(QStringLiteral(
                "INSERT INTO charging_orders(user_id, pile_id, status) "
                "VALUES(900001, 1, 'completed')")));
            QVERIFY(history.exec());
            QVERIFY(history.exec());
        });
    }

    void schemaRejectsBrokenRelations()
    {
        withDatabase([](QSqlDatabase database) {
            expectRejected(database,
                           "INSERT INTO charging_piles(station_id, code, type, power_kw) "
                           "VALUES(:station, :code, 'fast', 60)",
                           {{":station", 999999}, {":code", "ORPHAN-PILE"}});
            expectRejected(database,
                           "INSERT INTO charging_orders(user_id, pile_id, status) "
                           "VALUES(:user, 1, 'completed')",
                           {{":user", 999999}});
            expectRejected(database,
                           "INSERT INTO charging_orders(user_id, pile_id, status) "
                           "VALUES(900001, :pile, 'completed')",
                           {{":pile", 999999}});
            expectRejected(database,
                           "INSERT INTO charging_pricing_rules(station_id) VALUES(:station)",
                           {{":station", 999999}});
            expectRejected(database,
                           "INSERT INTO recharge_records(user_id, amount) VALUES(:user, 10)",
                           {{":user", 999999}});

            // 900001 号站的电桩被订单引用，删除必须被外键拦住
            expectRejected(database, "DELETE FROM charging_stations WHERE id = 900001");
            expectRejected(database, "DELETE FROM charging_piles WHERE id = 900002");
            expectRejected(database, "DELETE FROM users WHERE id = 900002");
        });
    }

    void repositoriesRejectInvalidInput()
    {
        withDatabase([](QSqlDatabase database) {
            charging::core::UserRepository users(database);
            charging::core::StationRepository stations(database);
            charging::core::PileRepository piles(database);
            QString error;

            const QStringList badPhones {"", "123", "1380013800", "138001380001",
                                         "1380013800a", "23800138000"};
            for (const auto& phone : badPhones) {
                error.clear();
                QVERIFY2(!users.loginOrCreate(phone, &error).has_value(),
                         qPrintable(QStringLiteral("应拒绝手机号 %1").arg(phone)));
                QVERIFY(!error.isEmpty());
            }

            const auto user = users.loginOrCreate(QStringLiteral("13800138000"), &error);
            QVERIFY2(user.has_value(), qPrintable(error));

            QVERIFY(!users.recharge(user->id, 0.0, &error));
            QVERIFY(!users.recharge(user->id, -50.0, &error));
            QCOMPARE(users.findById(user->id)->walletBalance, 0.0);

            QVERIFY(!users.updateNickname(user->id, QStringLiteral(""), &error));
            QVERIFY(!users.updateNickname(user->id, QStringLiteral("   "), &error));
            QVERIFY(!users.updateNickname(user->id, QString(31, QLatin1Char('a')), &error));
            QVERIFY(!users.updateAvatarPath(user->id, QString(501, QLatin1Char('b')), &error));
            QVERIFY(!users.setStatus(user->id, QStringLiteral("deleted"), &error));
            QVERIFY(!piles.updateStatus(1, QStringLiteral("broken"), &error));

            charging::core::ChargingStation incomplete;
            incomplete.address = QStringLiteral("地址");
            incomplete.pricePerKwh = 1.0;
            QVERIFY(!stations.create(incomplete, &error).has_value());

            incomplete.name = QStringLiteral("站");
            incomplete.address.clear();
            QVERIFY(!stations.create(incomplete, &error).has_value());

            incomplete.address = QStringLiteral("地址");
            incomplete.pricePerKwh = -1.0;
            QVERIFY(!stations.create(incomplete, &error).has_value());
        });
    }

    void activeOrderQuotaIsEnforced()
    {
        withDatabase([](QSqlDatabase database) {
            charging::core::UserRepository users(database);
            charging::core::OrderRepository orders(database);
            QString error;
            const auto first = users.loginOrCreate(QStringLiteral("13800138001"), &error);
            const auto second = users.loginOrCreate(QStringLiteral("13800138002"), &error);
            QVERIFY(first.has_value() && second.has_value());

            const auto order = orders.createReservation(first->id, 1, &error);
            QVERIFY2(order.has_value(), qPrintable(error));

            // 同一用户不能同时持有两个活动订单
            QVERIFY(!orders.createReservation(first->id, 2, &error).has_value());
            // 同一电桩不能被两个活动订单占用
            QVERIFY(!orders.createReservation(second->id, 1, &error).has_value());
            // 不存在的电桩不可预约
            QVERIFY(!orders.createReservation(second->id, 999999, &error).has_value());

            QVERIFY(orders.cancel(order->id, &error));
            // 取消后配额释放，两个用户都能重新预约
            QVERIFY(orders.createReservation(second->id, 1, &error).has_value());
        });
    }

    // ---------- 保存失败 ----------

    void failedWritesLeaveNoPartialData()
    {
        withDatabase([](QSqlDatabase database) {
            charging::core::UserRepository users(database);
            QString error;

            // 用户不存在时充值必须整体回滚，不能只留下充值流水
            QVERIFY(!users.recharge(999999, 100.0, &error));
            QVERIFY(!error.isEmpty());
            QCOMPARE(countRows(database, QStringLiteral("recharge_records")), 0);

            const auto user = users.loginOrCreate(QStringLiteral("13800138003"), &error);
            QVERIFY(user.has_value());
            QVERIFY(users.recharge(user->id, 50.0, &error));
            QCOMPARE(countRows(database, QStringLiteral("recharge_records")), 1);

            // 第二次充值失败不应污染第一次的流水
            QVERIFY(!users.recharge(999999, 10.0, &error));
            QCOMPARE(countRows(database, QStringLiteral("recharge_records")), 1);
            QCOMPARE(users.findById(user->id)->walletBalance, 50.0);
        });
    }

    void illegalOrderTransitionsAreRejected()
    {
        withDatabase([](QSqlDatabase database) {
            charging::core::UserRepository users(database);
            charging::core::OrderRepository orders(database);
            QString error;
            const auto user = users.loginOrCreate(QStringLiteral("13800138004"), &error);
            QVERIFY(users.recharge(user->id, 100.0, &error));
            const auto order = orders.createReservation(user->id, 1, &error);
            QVERIFY(order.has_value());

            // reserved 状态不能直接结束或结算
            QVERIFY(!orders.finishCharging(order->id, 1.0, 1.0, &error));
            QVERIFY(!orders.settle(order->id, &error));
            QCOMPARE(orders.findById(order->id)->status, QStringLiteral("reserved"));

            QVERIFY(orders.startCharging(order->id, &error));
            // charging 状态不能重复启动，也不能取消
            QVERIFY(!orders.startCharging(order->id, &error));
            QVERIFY(!orders.cancel(order->id, &error));
            QVERIFY(!orders.settle(order->id, &error));
            QCOMPARE(orders.findById(order->id)->status, QStringLiteral("charging"));

            QVERIFY(!orders.finishCharging(order->id, -1.0, 10.0, &error));
            QVERIFY(!orders.finishCharging(order->id, 10.0, -1.0, &error));
            QVERIFY(orders.finishCharging(order->id, 10.0, 20.0, &error));
            QVERIFY(orders.settle(order->id, &error));

            // completed 是终态
            QVERIFY(!orders.settle(order->id, &error));
            QVERIFY(!orders.cancel(order->id, &error));
            QVERIFY(!orders.startCharging(order->id, &error));
            QCOMPARE(orders.findById(order->id)->status, QStringLiteral("completed"));
            QCOMPARE(users.findById(user->id)->walletBalance, 80.0);

            QVERIFY(!orders.settle(999999, &error));
            QVERIFY(!orders.startCharging(999999, &error));
            QVERIFY(!orders.cancel(999999, &error));
        });
    }

    void updatesOnMissingRowsReportFailure()
    {
        withDatabase([](QSqlDatabase database) {
            charging::core::UserRepository users(database);
            charging::core::AdministratorRepository administrators(database);
            charging::core::PileRepository piles(database);
            QString error;
            QVERIFY(!users.updateNickname(999999, QStringLiteral("名字"), &error));
            QVERIFY(!users.updateAvatarPath(999999, QStringLiteral("path"), &error));
            QVERIFY(!users.setStatus(999999, QStringLiteral("active"), &error));
            QVERIFY(!users.recharge(999999, 10.0, &error));
            QVERIFY(!administrators.changePassword(999999, QStringLiteral("hash"), &error));
            QVERIFY(!administrators.updatePasswordHash(999999, QStringLiteral("hash"), &error));
            QVERIFY(!piles.updateStatus(999999, QStringLiteral("idle"), &error));
            QVERIFY(!piles.update(999999, QStringLiteral("fast"), 60.0, &error));
            error.clear();
            QVERIFY(!users.findByPhone(QStringLiteral("13800138099"), &error).has_value());
            QVERIFY(!users.findById(999999, &error).has_value());
            QVERIFY2(error.isEmpty(), "查不到记录属于正常结果，不应报错");
        });
    }

    // ---------- 同时操作 ----------

    void secondConnectionSeesCommittedDataOnly()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("shared.db"));

        charging::core::DatabaseManager writer(QStringLiteral("robust-writer-") + QUuid::createUuid().toString());
        charging::core::DatabaseManager reader(QStringLiteral("robust-reader-") + QUuid::createUuid().toString());
        QString error;
        QVERIFY2(writer.open(databasePath, &error), qPrintable(error));
        QVERIFY2(writer.initialize(&error), qPrintable(error));
        QVERIFY2(reader.open(databasePath, &error), qPrintable(error));

        {
            charging::core::UserRepository writerUsers(writer.database());
            charging::core::UserRepository readerUsers(reader.database());
            QVERIFY(!readerUsers.findByPhone(QStringLiteral("13800138005"), &error).has_value());

            const auto created = writerUsers.loginOrCreate(QStringLiteral("13800138005"), &error);
            QVERIFY2(created.has_value(), qPrintable(error));
            // 已提交的写入对另一条连接立即可见
            QVERIFY(readerUsers.findByPhone(QStringLiteral("13800138005"), &error).has_value());

            // 回滚的写入不能被另一条连接看到
            QSqlQuery rollbacked(writer.database());
            QVERIFY(rollbacked.exec(QStringLiteral("BEGIN")));
            QVERIFY(rollbacked.exec(QStringLiteral(
                "INSERT INTO users(phone, nickname) VALUES('13800138006', '未提交')")));
            QVERIFY(rollbacked.exec(QStringLiteral("ROLLBACK")));
            QVERIFY(!readerUsers.findByPhone(QStringLiteral("13800138006"), &error).has_value());
            QVERIFY(!writerUsers.findByPhone(QStringLiteral("13800138006"), &error).has_value());

            QVERIFY(writerUsers.recharge(created->id, 80.0, &error));
            QCOMPARE(readerUsers.findById(created->id, &error)->walletBalance, 80.0);
        }
    }

    void concurrentReservationOfSamePileHasSingleWinner()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("race.db"));

        qint64 firstUserId = 0;
        qint64 secondUserId = 0;
        {
            charging::core::DatabaseManager manager(uniqueConnection());
            QString error;
            QVERIFY2(manager.open(databasePath, &error), qPrintable(error));
            QVERIFY2(manager.initialize(&error), qPrintable(error));
            charging::core::UserRepository users(manager.database());
            firstUserId = users.loginOrCreate(QStringLiteral("13800138007"), &error)->id;
            secondUserId = users.loginOrCreate(QStringLiteral("13800138008"), &error)->id;
        }

        // 两条独立连接（各自线程）同时抢同一个空闲电桩
        QSemaphore arrived;
        QSemaphore start;
        ReservationRacer first(databasePath, firstUserId, 1, &arrived, &start);
        ReservationRacer second(databasePath, secondUserId, 1, &arrived, &start);
        first.start();
        second.start();
        arrived.acquire();
        arrived.acquire();
        start.release(2);
        QVERIFY(first.wait(30000));
        QVERIFY(second.wait(30000));

        QCOMPARE(first.succeeded + second.succeeded, 1);

        charging::core::DatabaseManager verifier(uniqueConnection());
        QString error;
        QVERIFY2(verifier.open(databasePath, &error), qPrintable(error));
        // 无论谁赢，库里都只能有一个活动订单，且电桩状态自洽
        QCOMPARE(countActiveOrdersForPile(verifier.database(), 1), 1);
        charging::core::PileRepository piles(verifier.database());
        QCOMPARE(piles.findById(1, &error)->status, QStringLiteral("idle"));

        const auto loser = first.succeeded ? &second : &first;
        QVERIFY2(!loser->error.isEmpty(), "失败方必须带回原因，而不是静默成功");
    }

    // ---------- 记录较多 ----------

    void handlesLargeVolumeOfRecords()
    {
        withDatabase([](QSqlDatabase database) {
            QString error;
            charging::core::UserRepository users(database);
            const auto user = users.loginOrCreate(QStringLiteral("13800138009"), &error);
            QVERIFY2(user.has_value(), qPrintable(error));

            constexpr int orderCount = 3000;
            constexpr int userCount = 1000;
            QElapsedTimer timer;
            timer.start();

            // 批量插入必须放在事务里，否则每条都单独 fsync
            QSqlQuery batch(database);
            QVERIFY(database.transaction());
            for (int index = 0; index < userCount; ++index) {
                batch.prepare(QStringLiteral(
                    "INSERT INTO users(phone, nickname, wallet_balance) "
                    "VALUES(:phone, :nick, 100)"));
                batch.bindValue(QStringLiteral(":phone"),
                                QStringLiteral("139%1").arg(index, 8, 10, QLatin1Char('0')));
                batch.bindValue(QStringLiteral(":nick"), QStringLiteral("批量用户%1").arg(index));
                QVERIFY2(batch.exec(), qPrintable(batch.lastError().text()));
            }

            QVector<qint64> orderIds;
            orderIds.reserve(orderCount);
            for (int index = 0; index < orderCount; ++index) {
                batch.prepare(QStringLiteral(
                    "INSERT INTO charging_orders(user_id, pile_id, status, energy_kwh, amount, "
                    "created_at) VALUES(:user, :pile, 'completed', 10, 12, "
                    "datetime('now', :offset))"));
                batch.bindValue(QStringLiteral(":user"), user->id);
                // 两个电桩轮流使用，避开"每桩一个活动订单"的部分唯一索引
                batch.bindValue(QStringLiteral(":pile"), index % 2 == 0 ? 1 : 2);
                batch.bindValue(QStringLiteral(":offset"),
                                QStringLiteral("-%1 minutes").arg(index));
                QVERIFY2(batch.exec(), qPrintable(batch.lastError().text()));
                orderIds.append(batch.lastInsertId().toLongLong());
            }
            QVERIFY(database.commit());
            const qint64 elapsedMs = timer.elapsed();

            QCOMPARE(countRows(database, QStringLiteral("users")), userCount + 5);
            QSqlQuery orders(database);
            QVERIFY(orders.exec(QStringLiteral("SELECT COUNT(*) FROM charging_orders")));
            QVERIFY(orders.next());
            // 3000 条新订单 + seed 里的 1 条演示订单
            QCOMPARE(orders.value(0).toInt(), orderCount + 1);

            charging::core::OrderRepository orderRepository(database);
            // 默认只取 50 条，且按时间倒序
            const auto defaultPage = orderRepository.listByUser(user->id, 50, &error);
            QCOMPARE(defaultPage.size(), 50);
            for (int index = 0; index < defaultPage.size(); ++index) {
                QCOMPARE(defaultPage[index].id, orderIds[index]);
            }
            // limit 被 qBound 夹到 1..200
            QCOMPARE(orderRepository.listByUser(user->id, 5000, &error).size(), 200);
            QCOMPARE(orderRepository.listByUser(user->id, 0, &error).size(), 1);
            QCOMPARE(orderRepository.listByUser(user->id, -10, &error).size(), 1);

            // 数据量变大后仍应能通过完整性检查
            charging::core::DatabaseMaintenance maintenance(database);
            const auto report = maintenance.checkIntegrity(&error);
            QVERIFY2(report.healthy, qPrintable(report.summary()));

            qDebug() << "批量写入" << userCount << "用户 +" << orderCount << "订单耗时"
                     << elapsedMs << "ms";
            // 宽松上限，只用于捕捉数量级退化，避免在慢机器上误报
            QVERIFY2(elapsedMs < 30000, qPrintable(QStringLiteral("批量写入耗时 %1 ms").arg(elapsedMs)));
        });
    }

private:
    // 两个线程各持一条连接，在同一时刻抢占同一个电桩
    class ReservationRacer final : public QThread {
    public:
        ReservationRacer(QString databasePath, qint64 userId, qint64 pileId, QSemaphore* arrived,
                         QSemaphore* start)
            : databasePath_(std::move(databasePath))
            , userId_(userId)
            , pileId_(pileId)
            , arrived_(arrived)
            , start_(start)
            , connectionName_(QStringLiteral("racer-") + QUuid::createUuid().toString())
        {
        }

        bool succeeded = false;
        QString error;

    protected:
        void run() override
        {
            {
                QSqlDatabase database =
                    QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
                database.setDatabaseName(databasePath_);
                if (!database.open()) {
                    error = database.lastError().text();
                } else {
                    QSqlQuery pragma(database);
                    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
                    pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));

                    arrived_->release();
                    start_->acquire();

                    charging::core::OrderRepository orders(database);
                    succeeded = orders.createReservation(userId_, pileId_, &error).has_value();
                    database.close();
                }
            }
            QSqlDatabase::removeDatabase(connectionName_);
        }

    private:
        QString databasePath_;
        qint64 userId_;
        qint64 pileId_;
        QSemaphore* arrived_;
        QSemaphore* start_;
        QString connectionName_;
    };

    static QString uniqueConnection()
    {
        return QStringLiteral("robustness-") + QUuid::createUuid().toString();
    }

    static void expectRejected(const QSqlDatabase& database, const QString& sql,
                               const QVariantMap& bindings = {})
    {
        QSqlQuery query(database);
        QVERIFY2(query.prepare(sql), qPrintable(query.lastError().text()));
        for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
            query.bindValue(it.key(), it.value());
        }
        QVERIFY2(!query.exec(),
                 qPrintable(QStringLiteral("数据库应拒绝该语句: %1").arg(sql)));
    }

    static int countRows(const QSqlDatabase& database, const QString& table)
    {
        QSqlQuery query(database);
        if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table)) || !query.next()) {
            return -1;
        }
        return query.value(0).toInt();
    }

    static int countActiveOrdersForPile(const QSqlDatabase& database, qint64 pileId)
    {
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM charging_orders WHERE pile_id = :pile "
            "AND status IN ('reserved','charging','awaiting_payment')"));
        query.bindValue(QStringLiteral(":pile"), pileId);
        if (!query.exec() || !query.next()) {
            return -1;
        }
        return query.value(0).toInt();
    }

    template<typename Function>
    static void withDatabase(Function function)
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        {
            charging::core::DatabaseManager manager(uniqueConnection());
            QString error;
            QVERIFY2(manager.open(directory.filePath(QStringLiteral("test.db")), &error),
                     qPrintable(error));
            QVERIFY2(manager.initialize(&error), qPrintable(error));
            function(manager.database());
        }
    }
};

QTEST_GUILESS_MAIN(DatabaseRobustnessTest)
#include "test_database_robustness.moc"
