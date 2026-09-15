# ZIP5 最新整合修复说明

请优先按 [运行与数据核对.md](运行与数据核对.md) 启动和核对。以下原有说明可能描述旧路径或旧数据口径。

# 第二阶段分析后端

本目录负责把 Spark SQL 生成的 10 张 ADS 结果表安全导入 MySQL，并通过 Flask 提供只读 JSON API。它与现有 Qt、SQLite 业务链路隔离，不修改交易数据。

## 数据链路

```text
CSV -> HDFS/ODS -> Spark SQL DWD -> DWS -> ADS -> MySQL charging_ads -> Flask -> Vue3/DataV
```

## Python 环境

只使用 Python 3.11 或 3.12：

```bash
cd /home/bit/charging-platform/analytics
python3.12 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
```

复制 `.env.example` 的变量到本机私有环境文件，不要把密码提交到 Git。

## Spark ADS 导出

在 Spark 虚拟机运行：

```bash
export ADS_EXPORT_DIR=/home/hadoop/temp/charging_ads_export
bash scripts/export_ads_local.sh
```

脚本只有在 10 张表全部成功导出后才替换正式导出目录；旧目录会被重命名保留，失败不会留下半套正式结果。

## MySQL 建表和导入

Spark 导出目录应包含 10 个无表头、竖线分隔的 CSV 文件，文件名与 ADS 表名一致。

首次在 MySQL 所在虚拟机以具备建库和用户管理权限的本地账号执行：

```bash
bash scripts/bootstrap_mysql_users.sh
```

脚本创建最小权限的 `charging_etl` 和只读 `charging_api` 用户；需要管理员认证时会在终端隐藏读取密码。随机业务密码包含大小写字母、数字和特殊字符，可通过 MySQL 默认密码策略，并且只保存在 `SECRETS_DIR` 指定的私有目录。

如果 Flask 与 MySQL 位于不同 VMware 网段，应在 MySQL 的 `[mysqld]` 配置段启用 `skip-name-resolve` 并重启 `mysqld`。否则 MySQL 可能因无法反向解析 NAT 来源地址而长时间不发送握手包。启用后账号必须使用 IP 或 `%` 主机规则；本项目创建的账号符合这一要求。

```bash
set -a
. /home/hadoop/ncs_data/secrets/analytics-etl.env
set +a
bash scripts/import_ads_mysql.sh
```

导入先写临时表，再在事务内替换正式表，并写入 `etl_batches`。任一文件缺失时不会修改正式结果。

## Flask API

```bash
export MYSQL_HOST=192.168.176.100
export MYSQL_USER=charging_api
export MYSQL_PASSWORD='replace_me'
python wsgi.py
```

统一响应字段为 `code`、`message`、`data`、`request_id`、`timestamp`。主要接口：

- `GET /api/v1/health`
- `GET /api/v1/dashboard`
- `GET /api/v1/overview`
- `GET /api/v1/users/levels`
- `GET /api/v1/users/radar`
- `GET /api/v1/platforms/distribution`
- `GET /api/v1/charging/hourly`
- `GET /api/v1/stations/types`
- `GET /api/v1/charging/week-compare`
- `GET /api/v1/battery/health`
- `GET /api/v1/areas/costs`
- `GET /api/v1/stations/top?limit=10`

## 测试

```bash
cd /home/bit/charging-platform/analytics
PYTHONPATH=. python -m unittest discover -s tests -v
```

单元测试通过依赖注入使用假结果库，不需要修改或启动真实 MySQL。

配置好 MySQL 环境变量后可执行全接口真实库冒烟测试：

```bash
PYTHONPATH=. python tests/smoke_real_mysql.py
```

Gunicorn 已启动时，可通过真实 HTTP 链路复测全部接口：

```bash
ANALYTICS_BASE_URL=http://127.0.0.1:8091 PYTHONPATH=. python tests/smoke_http.py
```

## Gunicorn 部署

先构建 Vue3/DataV 大屏：

```bash
cd /home/bit/charging-platform/web/dashboard
npm ci
npm run test
npm run build
```

Flask 会把构建结果挂载到 `/dashboard/`，根路径会重定向到该地址。可用
`DASHBOARD_DIST_DIR` 覆盖默认的 `web/dashboard/dist`，便于独立部署或测试。

前台联调运行：

```bash
cd /home/bit/charging-platform/analytics
set -a
. ./.env
set +a
.venv/bin/gunicorn -c deploy/gunicorn.conf.py wsgi:app
```

生产式开机启动可安装 `deploy/charging-analytics.service`。服务以 `bit` 用户运行，从 `/etc/charging-platform/analytics.env` 读取私有配置；安装环境文件时必须保持 `600` 权限。服务默认监听 `0.0.0.0:8091`。
