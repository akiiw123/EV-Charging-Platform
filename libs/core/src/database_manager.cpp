#include "charging/core/database_manager.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <utility>

namespace charging::core {
namespace {

QString readTextFile(const QString& path, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取 SQL 文件 %1: %2").arg(path, file.errorString());
        }
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

DatabaseManager::DatabaseManager(QString connectionName)
    : connectionName_(std::move(connectionName))
{
}

DatabaseManager::~DatabaseManager()
{
    if (database_.isValid()) {
        database_.close();
        database_ = {};
    }
    QSqlDatabase::removeDatabase(connectionName_);
}

bool DatabaseManager::open(const QString& databasePath, QString* errorMessage)
{
    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database_.setDatabaseName(databasePath);
    if (!database_.open()) {
        if (errorMessage) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    QSqlQuery pragma(database_);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
    return true;
}

bool DatabaseManager::initialize(QString* errorMessage)
{
    if (!isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("数据库尚未打开");
        }
        return false;
    }
    return executeScript(QStringLiteral(CHARGING_SCHEMA_PATH), errorMessage)
        && executeScript(QStringLiteral(CHARGING_SEED_PATH), errorMessage);
}

bool DatabaseManager::isOpen() const
{
    return database_.isValid() && database_.isOpen();
}

QSqlDatabase DatabaseManager::database() const
{
    return database_;
}

bool DatabaseManager::executeScript(const QString& path, QString* errorMessage)
{
    const QString script = readTextFile(path, errorMessage);
    if (script.isEmpty()) {
        return false;
    }

    if (!database_.transaction()) {
        if (errorMessage) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    for (const QString& statement : script.split(';', Qt::SkipEmptyParts)) {
        const QString sql = statement.trimmed();
        if (sql.isEmpty()) {
            continue;
        }
        QSqlQuery query(database_);
        if (!query.exec(sql)) {
            database_.rollback();
            if (errorMessage) {
                *errorMessage = QStringLiteral("SQL 执行失败: %1\n%2").arg(query.lastError().text(), sql);
            }
            return false;
        }
    }
    return database_.commit();
}

} // namespace charging::core
