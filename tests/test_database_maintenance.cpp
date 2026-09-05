#include "charging/core/database_maintenance.h"
#include "charging/core/database_manager.h"
#include "charging/core/repositories.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

// 覆盖小学期任务「数据维护：数据库备份和恢复」。
// 验证备份可用、恢复能找回被误删的数据，以及损坏检查能识别结构损坏与外键违例。
class DatabaseMaintenanceTest final : public QObject {
    Q_OBJECT

private slots:
    void healthyDatabasePassesIntegrityCheck()
    {
        withDatabase([](const QSqlDatabase& database) {
            charging::core::DatabaseMaintenance maintenance(database);
            QString error;
            const auto report = maintenance.checkIntegrity(&error);
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QVERIFY2(report.healthy, qPrintable(report.summary()));
            QCOMPARE(report.integrityLines, QStringList {QStringLiteral("ok")});
            QVERIFY(report.foreignKeyLines.isEmpty());
        });
    }

    void integrityCheckDetectsForeignKeyViolation()
    {
        withDatabase([](const QSqlDatabase& database) {
            QSqlQuery query(database);
            // 先关闭外键强制，才能人为造出一条引用不存在用户的脏数据
            QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys = OFF")));
            query.prepare(QStringLiteral(
                "INSERT INTO charging_orders(user_id, pile_id, status) "
                "VALUES(:user, :pile, 'completed')"));
            query.bindValue(QStringLiteral(":user"), 424242);
            query.bindValue(QStringLiteral(":pile"), 1);
            QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
            QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys = ON")));

            charging::core::DatabaseMaintenance maintenance(database);
            const auto report = maintenance.checkIntegrity();
            QVERIFY(!report.healthy);
            QVERIFY(!report.foreignKeyLines.isEmpty());
            QVERIFY(report.summary().contains(QStringLiteral("外键违例")));
        });
    }

    void backupCreatesVerifiableSnapshot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString backupPath = directory.filePath(QStringLiteral("snapshot.db"));

        withDatabase([&backupPath](const QSqlDatabase& database) {
            charging::core::UserRepository users(database);
            QString error;
            QVERIFY(users.loginOrCreate(QStringLiteral("13600136000"), &error).has_value());

            charging::core::DatabaseMaintenance maintenance(database);
            QVERIFY2(maintenance.backupTo(backupPath, &error), qPrintable(error));
            QVERIFY(QFile::exists(backupPath));
            QVERIFY2(charging::core::DatabaseMaintenance::verifyBackupFile(backupPath, &error),
                     qPrintable(error));

            // 备份是独立快照，不应被同一目标路径重复写入
            QVERIFY(!maintenance.backupTo(backupPath, &error));
            QVERIFY(error.contains(QStringLiteral("已存在")));
        });
    }

    void restoreRejectsMissingAndCorruptBackups()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString target = directory.filePath(QStringLiteral("live.db"));

        QString error;
        QVERIFY(!charging::core::DatabaseMaintenance::restoreFrom(
            directory.filePath(QStringLiteral("absent.db")), target, &error));
        QVERIFY(error.contains(QStringLiteral("不存在")));

        const QString corrupt = directory.filePath(QStringLiteral("corrupt.db"));
        QFile garbage(corrupt);
        QVERIFY(garbage.open(QIODevice::WriteOnly));
        // 空文件会被 SQLite 当成合法空库，必须写入真实垃圾字节
        garbage.write(QByteArray(4096, 'x'));
        garbage.close();

        QVERIFY(!charging::core::DatabaseMaintenance::verifyBackupFile(corrupt, &error));
        QVERIFY2(!error.isEmpty(), "拒绝恢复时必须说明原因");
        QVERIFY(!charging::core::DatabaseMaintenance::restoreFrom(corrupt, target, &error));
        QVERIFY(!QFile::exists(target));
    }

    void restoreRejectsTruncatedBackup()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("live.db"));
        const QString backupPath = directory.filePath(QStringLiteral("snapshot.db"));
        const QString truncatedPath = directory.filePath(QStringLiteral("truncated.db"));

        {
            charging::core::DatabaseManager manager(uniqueConnection());
            QString error;
            QVERIFY2(manager.open(databasePath, &error), qPrintable(error));
            QVERIFY2(manager.initialize(&error), qPrintable(error));
            charging::core::DatabaseMaintenance maintenance(manager.database());
            QVERIFY2(maintenance.backupTo(backupPath, &error), qPrintable(error));
        }

        QFile snapshot(backupPath);
        QVERIFY(snapshot.open(QIODevice::ReadOnly));
        const QByteArray snapshotBytes = snapshot.readAll();
        snapshot.close();
        QVERIFY(snapshotBytes.size() > 8192);

        QFile truncated(truncatedPath);
        QVERIFY(truncated.open(QIODevice::WriteOnly));
        // 砍掉后半段，模拟备份写到一半被中断
        truncated.write(snapshotBytes.left(snapshotBytes.size() / 2));
        truncated.close();

        QString error;
        QVERIFY(!charging::core::DatabaseMaintenance::verifyBackupFile(truncatedPath, &error));
        QVERIFY2(!error.isEmpty(), "截断的备份必须被识别为不可用");
        QVERIFY(!charging::core::DatabaseMaintenance::restoreFrom(
            truncatedPath, databasePath, &error));
    }

    void restoreRefusesWhileConnectionIsOpen()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("live.db"));
        const QString backupPath = directory.filePath(QStringLiteral("snapshot.db"));

        {
            charging::core::DatabaseManager manager(uniqueConnection());
            QString error;
            QVERIFY2(manager.open(databasePath, &error), qPrintable(error));
            QVERIFY2(manager.initialize(&error), qPrintable(error));
            charging::core::DatabaseMaintenance maintenance(manager.database());
            QVERIFY2(maintenance.backupTo(backupPath, &error), qPrintable(error));

            // 连接仍然打开时必须拒绝覆盖，否则后续读写会作用在已删除的文件上
            QVERIFY(!charging::core::DatabaseMaintenance::restoreFrom(
                backupPath, databasePath, &error));
            QVERIFY2(error.contains(QStringLiteral("占用")), qPrintable(error));
        }
    }

    void restoreRecoversDeletedData()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("live.db"));
        const QString backupPath = directory.filePath(QStringLiteral("snapshot.db"));

        {
            charging::core::DatabaseManager manager(uniqueConnection());
            QString error;
            QVERIFY2(manager.open(databasePath, &error), qPrintable(error));
            QVERIFY2(manager.initialize(&error), qPrintable(error));
            charging::core::UserRepository users(manager.database());
            QVERIFY(users.loginOrCreate(QStringLiteral("13600136000"), &error).has_value());

            charging::core::DatabaseMaintenance maintenance(manager.database());
            QVERIFY2(maintenance.backupTo(backupPath, &error), qPrintable(error));

            QSqlQuery wipe(manager.database());
            // 只删本测试新建的用户；整表删除会被订单外键正确拒绝
            wipe.prepare(QStringLiteral("DELETE FROM users WHERE phone = :phone"));
            wipe.bindValue(QStringLiteral(":phone"), QStringLiteral("13600136000"));
            QVERIFY2(wipe.exec(), qPrintable(wipe.lastError().text()));
            QCOMPARE(wipe.numRowsAffected(), 1);
            QVERIFY(!users.findByPhone(QStringLiteral("13600136000"), &error).has_value());
        }

        QString error;
        QVERIFY2(charging::core::DatabaseMaintenance::restoreFrom(backupPath, databasePath, &error),
                 qPrintable(error));

        {
            charging::core::DatabaseManager manager(uniqueConnection());
            QVERIFY2(manager.open(databasePath, &error), qPrintable(error));
            charging::core::UserRepository users(manager.database());
            const auto restored = users.findByPhone(QStringLiteral("13600136000"), &error);
            QVERIFY2(restored.has_value(), "恢复后应重新查到备份时存在的用户");

            charging::core::DatabaseMaintenance maintenance(manager.database());
            QVERIFY(maintenance.checkIntegrity(&error).healthy);
        }
    }

private:
    static QString uniqueConnection()
    {
        return QStringLiteral("maintenance-") + QUuid::createUuid().toString();
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

QTEST_GUILESS_MAIN(DatabaseMaintenanceTest)
#include "test_database_maintenance.moc"
