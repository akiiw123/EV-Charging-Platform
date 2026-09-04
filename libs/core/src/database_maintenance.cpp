#include "charging/core/database_maintenance.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

namespace charging::core {
namespace {

void setError(QString* target, const QString& message)
{
    if (target) {
        *target = message;
    }
}

// 在独立连接上执行 PRAGMA，返回每一行的首列文本
bool collectPragmaLines(QSqlDatabase database, const QString& pragma, QStringList* lines,
                        QString* errorMessage)
{
    QSqlQuery query(database);
    if (!query.exec(pragma)) {
        setError(errorMessage, QStringLiteral("%1 执行失败: %2").arg(pragma, query.lastError().text()));
        return false;
    }
    while (query.next()) {
        QStringList columns;
        const QSqlRecord record = query.record();
        for (int index = 0; index < record.count(); ++index) {
            columns << record.value(index).toString();
        }
        lines->append(columns.size() > 1 ? columns.join(QStringLiteral(" | ")) : columns.value(0));
    }
    return true;
}

// 返回正在使用指定数据库文件的连接名；没有则返回空串。
// 按绝对路径比较，兼容相对路径与不同分隔符写法。
QString findConnectionHoldingFile(const QString& databasePath)
{
    const QString target = QFileInfo(databasePath).absoluteFilePath();
    const QStringList names = QSqlDatabase::connectionNames();
    for (const QString& name : names) {
        const QSqlDatabase candidate = QSqlDatabase::database(name, /*open=*/false);
        if (candidate.isValid()
            && QFileInfo(candidate.databaseName()).absoluteFilePath() == target) {
            return name;
        }
    }
    return {};
}

} // namespace

QString IntegrityReport::summary() const
{
    if (healthy) {
        return QStringLiteral("数据库完整性检查通过");
    }
    QStringList problems;
    if (integrityLines.size() != 1
        || integrityLines.value(0).compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0) {
        problems << QStringLiteral("结构损坏: %1").arg(integrityLines.join(QStringLiteral("; ")));
    }
    if (!foreignKeyLines.isEmpty()) {
        problems << QStringLiteral("外键违例 %1 处").arg(foreignKeyLines.size());
    }
    return problems.isEmpty() ? QStringLiteral("数据库完整性检查未通过") : problems.join(QStringLiteral("；"));
}

DatabaseMaintenance::DatabaseMaintenance(QSqlDatabase database) : database_(std::move(database)) {}

bool DatabaseMaintenance::backupTo(const QString& destinationPath, QString* errorMessage) const
{
    if (destinationPath.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("备份路径不能为空"));
        return false;
    }
    if (!database_.isOpen()) {
        setError(errorMessage, QStringLiteral("数据库尚未打开"));
        return false;
    }
    if (QFile::exists(destinationPath)) {
        setError(errorMessage, QStringLiteral("备份目标 %1 已存在，请换一个文件名").arg(destinationPath));
        return false;
    }

    const QFileInfo destination(destinationPath);
    QDir parent = destination.dir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        setError(errorMessage, QStringLiteral("无法创建备份目录 %1").arg(parent.absolutePath()));
        return false;
    }

    // VACUUM INTO 由 SQLite 在单个原子步骤内生成快照，并且顺带完成碎片整理。
    // 它不能在事务中执行，因此这里直接以自动提交方式运行。
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("VACUUM INTO :destination"));
    query.bindValue(QStringLiteral(":destination"), destination.absoluteFilePath());
    if (!query.exec()) {
        setError(errorMessage, QStringLiteral("备份失败: %1").arg(query.lastError().text()));
        return false;
    }
    return QFile::exists(destination.absoluteFilePath());
}

IntegrityReport DatabaseMaintenance::checkIntegrity(QString* errorMessage) const
{
    IntegrityReport report;
    if (!database_.isOpen()) {
        setError(errorMessage, QStringLiteral("数据库尚未打开"));
        return report;
    }
    if (!collectPragmaLines(database_, QStringLiteral("PRAGMA integrity_check"),
                            &report.integrityLines, errorMessage)) {
        return report;
    }
    if (!collectPragmaLines(database_, QStringLiteral("PRAGMA foreign_key_check"),
                            &report.foreignKeyLines, errorMessage)) {
        return report;
    }
    report.healthy = report.integrityLines.size() == 1
        && report.integrityLines.first().compare(QStringLiteral("ok"), Qt::CaseInsensitive) == 0
        && report.foreignKeyLines.isEmpty();
    return report;
}

bool DatabaseMaintenance::verifyBackupFile(const QString& backupPath, QString* errorMessage)
{
    if (!QFile::exists(backupPath)) {
        setError(errorMessage, QStringLiteral("备份文件 %1 不存在").arg(backupPath));
        return false;
    }

    const QString connection = QStringLiteral("charging-verify-") + QUuid::createUuid().toString();
    bool healthy = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(backupPath);
        if (!database.open()) {
            setError(errorMessage, QStringLiteral("无法打开备份文件: %1").arg(database.lastError().text()));
        } else {
            QStringList lines;
            QString pragmaError;
            if (!collectPragmaLines(database, QStringLiteral("PRAGMA integrity_check"), &lines,
                                    &pragmaError)) {
                // 非数据库文件（例如被文本覆盖）在 SQLite 里能"打开"，但一读就报错
                setError(errorMessage, QStringLiteral("备份文件不可用: %1").arg(pragmaError));
            } else {
                healthy = lines.size() == 1
                    && lines.first().compare(QStringLiteral("ok"), Qt::CaseInsensitive) == 0;
                if (!healthy) {
                    setError(errorMessage,
                             QStringLiteral("备份文件已损坏: %1").arg(lines.join(QStringLiteral("; "))));
                }
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return healthy;
}

bool DatabaseMaintenance::restoreFrom(const QString& backupPath, const QString& databasePath,
                                      QString* errorMessage)
{
    if (!verifyBackupFile(backupPath, errorMessage)) {
        return false;
    }
    if (databasePath.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("目标数据库路径不能为空"));
        return false;
    }

    // 仍有连接打开时覆盖文件会让后续读写作用在已删除的 inode 上，必须先拒绝
    const QString holder = findConnectionHoldingFile(databasePath);
    if (!holder.isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("数据库仍被连接 %1 占用，请先关闭连接再恢复").arg(holder));
        return false;
    }

    // WAL 模式下残留的 -wal / -shm 会在下次打开时被重放，覆盖掉刚恢复的内容
    QFile::remove(databasePath + QStringLiteral("-wal"));
    QFile::remove(databasePath + QStringLiteral("-shm"));

    if (QFile::exists(databasePath) && !QFile::remove(databasePath)) {
        setError(errorMessage, QStringLiteral("无法删除现有数据库文件 %1").arg(databasePath));
        return false;
    }
    if (!QFile::copy(backupPath, databasePath)) {
        setError(errorMessage, QStringLiteral("无法把备份复制到 %1").arg(databasePath));
        return false;
    }
    // QFile::copy 会沿用备份文件的权限，备份常以只读方式保存
    QFile::setPermissions(databasePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                            | QFileDevice::ReadUser | QFileDevice::WriteUser);
    return true;
}

} // namespace charging::core
