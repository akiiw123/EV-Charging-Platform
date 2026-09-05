#include "charging/core/database_manager.h"
#include "charging/core/repositories.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

class RepositoryTest final : public QObject {
    Q_OBJECT

private slots:
    void initDatabaseAndSeedAdministrator()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::AdministratorRepository administrators(database);
            const auto admin = administrators.findByUsername(QStringLiteral("admin"));
            QVERIFY(admin.has_value());
            QCOMPARE(admin->username, QStringLiteral("admin"));
        });
    }

    void createUserAndRecharge()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::UserRepository users(database);
            QString error;
            const auto user = users.loginOrCreate(QStringLiteral("13800138000"), &error);
            QVERIFY2(user.has_value(), qPrintable(error));
            QCOMPARE(user->nickname, QStringLiteral("用户8000"));
            QCOMPARE(users.loginOrCreate(QStringLiteral("13800138000"))->id, user->id);
            QVERIFY(users.recharge(user->id, 100.0, &error));
            QCOMPARE(users.findById(user->id)->walletBalance, 100.0);
            QVERIFY(!users.loginOrCreate(QStringLiteral("123"), &error).has_value());
        });
    }

    void createStationAndPile()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::StationRepository stations(database);
            charging::core::PileRepository piles(database);
            QString error;
            charging::core::ChargingStation input;
            input.name = QStringLiteral("测试站");
            input.address = QStringLiteral("测试地址");
            input.latitude = 41.8;
            input.longitude = 123.4;
            input.pricePerKwh = 1.5;
            const auto station = stations.create(input, &error);
            QVERIFY2(station.has_value(), qPrintable(error));
            charging::core::ChargingPile pile;
            pile.stationId = station->id;
            pile.code = QStringLiteral("TEST-001");
            pile.type = QStringLiteral("fast");
            pile.powerKw = 120;
            const auto createdPile = piles.create(pile, &error);
            QVERIFY2(createdPile.has_value(), qPrintable(error));
            QCOMPARE(piles.listByStation(station->id).size(), 1);
        });
    }

    void orderLifecycleIsTransactional()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::UserRepository users(database);
            charging::core::PileRepository piles(database);
            charging::core::OrderRepository orders(database);
            QString error;
            const auto user = users.loginOrCreate(QStringLiteral("13900139000"), &error);
            QVERIFY(user.has_value());
            QVERIFY(users.recharge(user->id, 50.0, &error));
            const auto pile = piles.listByStation(1).first();
            const auto order = orders.createReservation(user->id, pile.id, &error);
            QVERIFY2(order.has_value(), qPrintable(error));
            QVERIFY(!orders.createReservation(user->id, pile.id, &error).has_value());
            QVERIFY(orders.startCharging(order->id, &error));
            QCOMPARE(piles.findById(pile.id)->status, QStringLiteral("charging"));
            QVERIFY(orders.finishCharging(order->id, 10.0, 20.0, &error));
            QCOMPARE(piles.findById(pile.id)->status, QStringLiteral("idle"));
            QVERIFY(orders.settle(order->id, &error));
            QCOMPARE(orders.findById(order->id)->status, QStringLiteral("completed"));
            QCOMPARE(users.findById(user->id)->walletBalance, 30.0);
        });
    }

    void insufficientBalanceRollsBack()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::UserRepository users(database);
            charging::core::PileRepository piles(database);
            charging::core::OrderRepository orders(database);
            QString error;
            const auto user = users.loginOrCreate(QStringLiteral("13700137000"), &error);
            const auto pile = piles.listByStation(1).first();
            const auto order = orders.createReservation(user->id, pile.id, &error);
            QVERIFY(orders.startCharging(order->id, &error));
            QVERIFY(orders.finishCharging(order->id, 5.0, 10.0, &error));
            QVERIFY(!orders.settle(order->id, &error));
            QCOMPARE(orders.findById(order->id)->status, QStringLiteral("awaiting_payment"));
            QCOMPARE(users.findById(user->id)->walletBalance, 0.0);
        });
    }

private:
    template<typename Function>
    static void withDatabase(Function function)
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString connection = QStringLiteral("test-") + QUuid::createUuid().toString();
        {
            charging::core::DatabaseManager manager(connection);
            QString error;
            QVERIFY2(manager.open(directory.filePath(QStringLiteral("test.db")), &error), qPrintable(error));
            QVERIFY2(manager.initialize(&error), qPrintable(error));
            function(manager.database());
        }
    }
};

// 使用 QSqlDatabase 必须有 QCoreApplication；QTEST_APPLESS_MAIN 会直接段错误
QTEST_GUILESS_MAIN(RepositoryTest)
#include "test_repositories.moc"
