#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

namespace charging::core {

// PRAGMA integrity_check 与 PRAGMA foreign_key_check 的合并结果
struct IntegrityReport {
    bool healthy = false;
    QStringList integrityLines;    // 健康时为单行 "ok"
    QStringList foreignKeyLines;   // 健康时为空
    QString summary() const;
};

// 数据库备份、恢复与损坏检查。操作步骤见 docs/database-maintenance.md。
class DatabaseMaintenance final {
public:
    explicit DatabaseMaintenance(QSqlDatabase database);

    // 使用 SQLite 的 VACUUM INTO 生成一致性快照，会在单个原子操作内完成，
    // 因此不需要先停服务。目标文件必须不存在，父目录会自动创建。
    bool backupTo(const QString& destinationPath, QString* errorMessage = nullptr) const;

    // 检查库文件是否损坏以及是否存在外键违例，不修改数据
    IntegrityReport checkIntegrity(QString* errorMessage = nullptr) const;

    // 恢复前校验备份文件自身完整性，避免用坏备份覆盖好库
    static bool verifyBackupFile(const QString& backupPath, QString* errorMessage = nullptr);

    // 用备份覆盖数据库文件。调用前必须关闭该库的全部连接（进程内会检测并拒绝）。
    // 会一并删除残留的 -wal / -shm 边车文件，防止旧 WAL 覆盖恢复结果。
    static bool restoreFrom(const QString& backupPath, const QString& databasePath,
                            QString* errorMessage = nullptr);

private:
    QSqlDatabase database_;
};

} // namespace charging::core
