#include "charging/core/database_manager.h"
#include "charging/core/repositories.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

// 数据库版本迁移回归测试。
//
// 背景：线上出现过管理端启动失败——旧版本创建的 charging_platform.db 中
// charging_stations 表没有 status 列；schema.sql 的 CREATE TABLE IF NOT EXISTS
// 对已存在的旧表是 no-op，补列只能靠编译进二进制的 DatabaseManager::migrate()
// （v5 迁移）。一旦"磁盘上的新 SQL 文件"与"旧可执行文件"发生版本漂移，
// seed.sql 的 UPDATE charging_stations SET status=... 就会报
// "no such column: status" 并使 initialize() 失败、管理端退出。
//
// 本测试用手工建旧结构库的方式固化该不变量：当前代码的 initialize()
// 必须能把缺 status 列的旧库升级成功并完整跑完 seed，且不得丢失存量数据。
class DatabaseMigrationTest final : public QObject {
    Q_OBJECT

private slots:
    // 旧库（charging_stations 无 status 列 + 存量站点）经 initialize() 后：
    // 1) migrate() 补出 status 列；2) 存量行取默认值 active 且不丢失；
    // 3) seed 完整执行，900014 被置为 disabled。
    void legacyDatabaseWithoutStatusColumnIsUpgraded()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("legacy.db"));

        // 阶段一：手工构造 v5 之前的旧结构库（charging_stations 无 status 列）
        {
            charging::core::DatabaseManager legacy(QStringLiteral("legacy-") + QUuid::createUuid().toString());
            QString error;
            QVERIFY2(legacy.open(path, &error), qPrintable(error));
            QSqlQuery query(legacy.database());
            QVERIFY2(query.exec(QStringLiteral(
                             "CREATE TABLE charging_stations ("
                             "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                             "name TEXT NOT NULL,"
                             "address TEXT NOT NULL,"
                             "latitude REAL NOT NULL,"
                             "longitude REAL NOT NULL,"
                             "price_per_kwh REAL NOT NULL CHECK(price_per_kwh >= 0),"
                             "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)")),
                     qPrintable(query.lastError().text()));
            QVERIFY2(query.exec(QStringLiteral(
                             "INSERT INTO charging_stations(id,name,address,latitude,longitude,price_per_kwh) "
                             "VALUES(1,'旧版存量电站','旧版地址',39.90,116.40,1.20)")),
                     qPrintable(query.lastError().text()));
        }

        // 阶段二：用当前代码 initialize()。schema 对旧表 no-op，
        // migrate() 必须补列，seed（含依赖 status 的 UPDATE）必须跑完。
        charging::core::DatabaseManager current(QStringLiteral("migrated-") + QUuid::createUuid().toString());
        QString error;
        QVERIFY2(current.open(path, &error), qPrintable(error));
        QVERIFY2(current.initialize(&error), qPrintable(error));

        QSqlQuery query(current.database());
        QVERIFY(query.exec(QStringLiteral("SELECT name,status FROM charging_stations WHERE id=1")));
        QVERIFY(query.next());
        // 存量数据不丢失，且补列默认值为 active
        QCOMPARE(query.value(0).toString(), QStringLiteral("旧版存量电站"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("active"));

        QVERIFY(query.exec(QStringLiteral("SELECT status FROM charging_stations WHERE id=900014")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("disabled"));

        // 仓储层按新结构读取正常
        charging::core::StationRepository stations(current.database());
        const auto station = stations.findById(1, &error);
        QVERIFY2(station.has_value(), qPrintable(error));
        QCOMPARE(station->status, QStringLiteral("active"));
    }

    // 同一数据库连续 initialize() 两次必须都成功（迁移与 seed 均幂等），
    // 防止"重启一次就坏"的回归。
    void initializeIsIdempotentOnSameDatabase()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("double.db"));

        for (int round = 0; round < 2; ++round) {
            charging::core::DatabaseManager manager(QStringLiteral("double-%1-").arg(round) + QUuid::createUuid().toString());
            QString error;
            QVERIFY2(manager.open(path, &error), qPrintable(error));
            QVERIFY2(manager.initialize(&error), qPrintable(error));
        }

        charging::core::DatabaseManager verifier(QStringLiteral("double-verify-") + QUuid::createUuid().toString());
        QString error;
        QVERIFY2(verifier.open(path, &error), qPrintable(error));
        charging::core::StationRepository stations(verifier.database());
        const auto disabled = stations.findById(900014, &error);
        QVERIFY2(disabled.has_value(), qPrintable(error));
        QCOMPARE(disabled->status, QStringLiteral("disabled"));
        const auto active = stations.findById(1, &error);
        QVERIFY2(active.has_value(), qPrintable(error));
        QCOMPARE(active->status, QStringLiteral("active"));
    }

    // 全新数据库必须直接带上 status 列并完成 seed（对照组，
    // 用于区分"迁移失败"与"schema/seed 本身损坏"）。
    void freshDatabaseGetsStatusColumnAndSeed()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        charging::core::DatabaseManager manager(QStringLiteral("fresh-") + QUuid::createUuid().toString());
        QString error;
        QVERIFY2(manager.open(directory.filePath(QStringLiteral("fresh.db")), &error), qPrintable(error));
        QVERIFY2(manager.initialize(&error), qPrintable(error));

        charging::core::StationRepository stations(manager.database());
        const auto disabled = stations.findById(900014, &error);
        QVERIFY2(disabled.has_value(), qPrintable(error));
        QCOMPARE(disabled->status, QStringLiteral("disabled"));
    }
};

QTEST_GUILESS_MAIN(DatabaseMigrationTest)
#include "test_database_migration.moc"
