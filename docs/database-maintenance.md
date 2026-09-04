# 数据库维护：备份、恢复与损坏检查

本文是数据库端的运维操作说明，覆盖三件事：**备份**、**恢复**、**损坏检查**。
每一步都给出两种做法：程序内 API（`charging::core::DatabaseMaintenance`）和
命令行（`sqlite3`），运维时可以只用后者，不需要重新编译。

## 1. 对象与位置

管理服务端在启动时打开数据库，文件位于**服务端进程的工作目录**：

```cpp
// apps/admin-server/src/main.cpp
const QString databasePath = QDir::current().filePath(QStringLiteral("charging_platform.db"));
```

数据库使用 WAL 日志模式，因此运行期间目录里可能出现三个文件：

| 文件 | 说明 | 能否单独拷贝 |
|---|---|---|
| `charging_platform.db` | 主库文件 | 不能，见下 |
| `charging_platform.db-wal` | 未合并回主库的写入日志 | 不能 |
| `charging_platform.db-shm` | WAL 共享内存索引 | 不能 |

**关键限制：WAL 模式下直接 `cp` 主库文件会得到不一致的快照。**
最近提交的数据可能还留在 `-wal` 里没有合并，拷出来的文件可能缺数据，
甚至打不开。备份必须走下面第 2 节的原子接口。

连接在干净关闭时 SQLite 会自动做 checkpoint 并删除 `-wal` / `-shm`，
所以服务端停止后目录里通常只剩主库文件；但只要还有连接打开，就必须假定边车文件存在。

## 2. 备份

### 2.1 程序内 API

```cpp
#include "charging/core/database_maintenance.h"

charging::core::DatabaseMaintenance maintenance(databaseManager.database());
QString error;
if (!maintenance.backupTo(QStringLiteral("/var/backups/charging/2026-09-04.db"), &error)) {
    qCritical().noquote() << error;
}
```

`backupTo` 的行为：

- 底层执行 `VACUUM INTO`，由 SQLite 在**单个原子步骤**内生成一致性快照，
  因此**不需要先停服务**，在线备份是安全的。
- 顺带完成碎片整理，备份文件通常比主库小。
- **目标文件必须不存在**，否则返回 `备份目标 ... 已存在，请换一个文件名`。
  这是刻意设计：防止误覆盖上一份可用备份。
- 父目录不存在时会自动创建。
- `VACUUM INTO` 不能在事务中执行，实现里以自动提交方式运行。

### 2.2 命令行

两种写法都是原子的，任选其一：

```bash
# 写法一：VACUUM INTO（与程序内 API 完全等价）
sqlite3 charging_platform.db "VACUUM INTO '/var/backups/charging/2026-09-04.db'"

# 写法二：.backup 在线备份 API
sqlite3 charging_platform.db ".backup '/var/backups/charging/2026-09-04.db'"
```

不要用 `cp charging_platform.db backup.db`，原因见第 1 节。

### 2.3 备份后立即校验

备份文件本身也可能是坏的（磁盘满、传输截断），**校验通过的备份才算备份**：

```cpp
QString error;
if (!charging::core::DatabaseMaintenance::verifyBackupFile(path, &error)) {
    qCritical().noquote() << error;   // 这份备份不可用于恢复
}
```

```bash
sqlite3 '/var/backups/charging/2026-09-04.db' "PRAGMA integrity_check;"
# 期望输出恰好一行：ok
```

`verifyBackupFile` 会把两类问题区分开：

- **不是数据库文件**（例如被文本覆盖、传输协议出错）：SQLite 能“打开”它，
  但一执行 PRAGMA 就报 `file is not a database`，返回
  `备份文件不可用: PRAGMA integrity_check 执行失败: ...`。
- **是数据库但结构损坏**（例如被截断）：返回
  `备份文件已损坏: ...`，CLI 下对应 `database disk image is malformed`。

### 2.4 定时备份

服务端未内置定时器，用系统 cron 调用一个脚本即可。
不要把整条链塞进 crontab 单行：cron 里 `%` 必须转义成 `\%`，
且命令行不支持反斜杠续行，很容易写错。

下面的脚本已实测，做的是**备份 → 校验 → 只有校验通过才清理旧备份**：

```bash
#!/usr/bin/env bash
# /opt/charging/backup.sh
set -euo pipefail
DB="${1:?用法: backup.sh <数据库文件> <备份目录> [保留天数]}"
BACKUP_DIR="${2:?缺少备份目录}"
KEEP_DAYS="${3:-14}"

STAMP="$(date +%Y%m%d-%H%M%S)"
DEST="$BACKUP_DIR/charging-$STAMP.db"
mkdir -p "$BACKUP_DIR"

sqlite3 "$DB" "VACUUM INTO '$DEST'"

if [ "$(sqlite3 "$DEST" 'PRAGMA integrity_check;')" = "ok" ]; then
    echo "备份成功并已校验: $DEST"
    find "$BACKUP_DIR" -name 'charging-*.db' -mtime "+$KEEP_DAYS" -delete
else
    echo "备份校验失败，已保留旧备份: $DEST" >&2
    exit 1
fi
```

`set -e` 保证 `VACUUM INTO` 失败时立即中止，不会走到清理那一步；
校验不通过时同样以非零退出，旧备份不会被删——避免坏备份把好的挤掉。

cron 只负责按点调用（每天 03:30，日志追加到文件）：

```bash
# /etc/cron.d/charging-backup
30 3 * * * charging /opt/charging/backup.sh /opt/charging/charging_platform.db /var/backups/charging 14 >> /var/log/charging-backup.log 2>&1
```

备份目录应与主库**不在同一块磁盘**，否则磁盘故障会同时损失两份数据。

## 3. 恢复

恢复会用备份**整体覆盖**当前数据库，覆盖之后到备份时间点为止的所有数据都会丢失。
执行前确认这是你想要的结果。

### 3.1 前置条件

1. **停止管理服务端**。恢复要求目标库没有任何打开的连接：
   - 程序内 API 会遍历 `QSqlDatabase::connectionNames()`，按绝对路径找到占用者并拒绝，
     返回 `数据库仍被连接 <name> 占用，请先关闭连接再恢复`。
   - 命令行下自行确认进程已退出（`pgrep -a admin-server`）。
2. **校验备份**（第 2.3 节）。`restoreFrom` 内部会先调 `verifyBackupFile`，
   校验不过直接返回，不会碰目标文件——这是防止“用坏备份覆盖好库”的关键一步。
3. 如条件允许，先把当前（可能已损坏的）库另存一份用于事后排查：
   `mv charging_platform.db charging_platform.db.broken`。

### 3.2 程序内 API

```cpp
QString error;
if (!charging::core::DatabaseMaintenance::restoreFrom(
        QStringLiteral("/var/backups/charging/2026-09-04.db"),
        QStringLiteral("charging_platform.db"), &error)) {
    qCritical().noquote() << error;
}
```

`restoreFrom` 依次做四件事：

1. 校验备份文件完整性，不通过则中止。
2. 检查是否有连接占用目标库，有则中止。
3. **删除残留的 `-wal` 和 `-shm`**。这一步不可省略：WAL 模式下旧的
   `-wal` 会在下次打开时被重放到新库上，把刚恢复的内容覆盖掉，
   表现为“恢复成功了但数据还是旧的”。
4. 删除目标文件并复制备份过去，随后恢复读写权限
   （`QFile::copy` 会沿用备份文件的权限，而备份常以只读方式保存）。

### 3.3 命令行

```bash
# 1) 停服务
systemctl stop charging-admin      # 或直接结束进程

# 2) 校验备份
sqlite3 '/var/backups/charging/2026-09-04.db' "PRAGMA integrity_check;"   # 必须输出 ok

# 3) 清掉 WAL 边车文件（即使当前看不到也要执行）
rm -f charging_platform.db-wal charging_platform.db-shm

# 4) 覆盖
cp -f '/var/backups/charging/2026-09-04.db' charging_platform.db
chmod u+rw charging_platform.db

# 5) 启动并自检
systemctl start charging-admin
sqlite3 charging_platform.db "PRAGMA integrity_check; PRAGMA foreign_key_check;"
```

### 3.4 恢复后验证

恢复出来的库结构完整不代表业务数据符合预期，至少确认：

```bash
sqlite3 charging_platform.db "SELECT count(*) FROM users;
SELECT count(*) FROM charging_orders;
SELECT count(*) FROM charging_pricing_periods;"
```

服务端启动时会自动执行 `database/schema.sql` 与 `database/seed.sql`，
两者都是幂等的（`CREATE TABLE IF NOT EXISTS` / `INSERT OR IGNORE`），
所以**恢复到旧版本的库后再启动，会自动补齐缺失的表和演示数据**，不需要手工迁移。

## 4. 损坏检查

### 4.1 两项检查缺一不可

```cpp
charging::core::DatabaseMaintenance maintenance(databaseManager.database());
QString error;
const auto report = maintenance.checkIntegrity(&error);
qInfo().noquote() << report.summary();
if (!report.healthy) {
    qWarning().noquote() << report.integrityLines << report.foreignKeyLines;
}
```

`checkIntegrity` 合并执行 `PRAGMA integrity_check` 与 `PRAGMA foreign_key_check`：

- `integrityLines`：健康时恰好一行 `ok`；否则是 SQLite 报出的具体损坏描述。
- `foreignKeyLines`：健康时为空；每行形如 `子表 | 行id | 父表 | 父键序号`。
- `healthy` 要求前者为单行 `ok` **且**后者为空。
- `summary()` 返回可直接展示给运维的中文结论。

**为什么两项都要跑：`integrity_check` 只检查物理结构，不检查引用关系。**
一个存在孤儿订单行的库，`integrity_check` 仍然返回 `ok`。实测：

```bash
sqlite3 fk.db "PRAGMA foreign_keys=OFF;
CREATE TABLE p(id INTEGER PRIMARY KEY);
CREATE TABLE c(id INTEGER PRIMARY KEY, pid INTEGER REFERENCES p(id));
INSERT INTO c(pid) VALUES(999);"

sqlite3 fk.db "PRAGMA integrity_check;"      # ok  ← 看不出问题
sqlite3 fk.db "PRAGMA foreign_key_check;"    # c|1|p|0  ← 孤儿行在这里暴露
```

外键违例通常意味着有人绕过应用直接改过库，或在 `PRAGMA foreign_keys=OFF`
的连接上执行过写入（SQLite 的外键约束默认关闭，本项目在
`DatabaseManager::open` 里显式打开）。

### 4.2 命令行

```bash
sqlite3 charging_platform.db "PRAGMA integrity_check;"      # 期望：ok
sqlite3 charging_platform.db "PRAGMA foreign_key_check;"    # 期望：无输出
sqlite3 charging_platform.db "PRAGMA quick_check;"          # 大库快速筛查，期望：ok
```

`integrity_check` 会遍历全部页面，库很大时较慢；日常巡检可先用 `quick_check`，
出问题时再跑完整检查。

### 4.3 检查频率与处置

| 时机 | 动作 |
|---|---|
| 每次备份后 | 对备份文件跑 `integrity_check`（第 2.3 节） |
| 每日巡检 | 对主库跑 `quick_check` |
| 服务端启动异常、出现无法解释的 `DATABASE_ERROR` | 跑完整 `integrity_check` + `foreign_key_check` |
| 检查不通过 | 停止写入，用最近一份校验通过的备份恢复（第 3 节） |

`integrity_check` 报出结构损坏时**不要继续写入**：SQLite 在损坏库上的写入
可能进一步扩大损坏范围，让原本可恢复的数据变得不可恢复。

## 5. 常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| `备份目标 ... 已存在` | 目标文件名重复 | 换带日期/时间戳的文件名，不要覆盖旧备份 |
| `数据库仍被连接 xxx 占用` | 服务端未停止 | 停服务后重试 |
| 恢复成功但数据还是旧的 | 未删除 `-wal` / `-shm` | 按第 3.2/3.3 节删除边车文件后重新恢复 |
| `备份文件不可用: ... file is not a database` | 备份被非数据库内容覆盖 | 该备份作废，换上一份 |
| `备份文件已损坏: ... malformed` | 备份被截断或磁盘出错 | 该备份作废，换上一份 |
| `integrity_check` 为 `ok` 但 `foreign_key_check` 有输出 | 存在孤儿行，非物理损坏 | 定位并修正业务数据，不需要整库恢复 |
| 恢复后文件只读、服务端写失败 | 备份以只读保存，`cp` 沿用了权限 | `chmod u+rw charging_platform.db`（API 已自动处理） |

## 6. 已知限制

- 服务端**未内置**备份定时器与管理界面，定时备份依赖系统 cron。
- `restoreFrom` 只覆盖单个库文件，不负责把备份从远端拉回来；
  异机备份的传输由运维流程保证。
- `DatabaseMaintenance` 的实例方法依赖传入的 `QSqlDatabase` 连接，
  Qt SQL 连接是线程私有的，**不要把同一个连接跨线程使用**；
  在工作线程里做维护操作时，用该线程自己的连接构造对象。
- `backupTo` 与 `checkIntegrity` 在线可用；`restoreFrom` 必须在无连接占用的
  前提下执行，即实际需要停服务。
- 本文的 CLI 示例已在 `sqlite3 3.50.4` 上逐条实测；程序内 API 的行为由
  `tests/test_database_maintenance.cpp`（9 个用例）覆盖，包含损坏备份、
  截断备份、连接占用拒绝和恢复后数据校验。
