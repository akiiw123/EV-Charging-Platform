# 大屏与数据库交接文档

更新日期：2026-09-15。面向继续维护前端、分析链路和部署的同学。

项目内位置：`docs/dashboard-data-handoff.md`。本文相对链接以该目录为基准。

## 1. 接手先看这几条

1. **唯一开发目录是 Ubuntu 虚拟机 `/home/bit/EV-Charging-Platform`。** Windows 使用 `ssh BitDev` 连接；不要在 Windows 旧副本开发或用它判断现状。
2. 当前分支是 `glm`。PR #23（`4c65d2c`）已同步，保留新布局的整合提交为 `1bbaff9`。接手时重新检查分支、工作区和远程，本文不是实时状态。
3. **当前布局是用户希望保留的版本：**中央沉浸地图、折叠 KPI、两侧展开卡片、“用户与时段 / 结构与收益”分组、左下排行和右上负载/收入关系图。修功能时不要直接用 `main` 的旧 App.vue/styles.css 覆盖。
4. **页面能打开不等于真实分析链路已打通。** 最近检查中，8091 的旧进程没有新版 metadata 接口，响应与测试样例一致；独立读取私有 `.env` 后连接 MySQL 未成功。真实数据仍待核验。
5. 地图站点来自 SQLite 业务库的 `/api/v1/live/stations`，与实时 KPI 使用同一来源；Spark/MySQL 历史批次仍是独立分析来源。
6. 本次工作没有替换旧 Qt 大屏入口、重启分析后端或修改真实数据库；前端构建已更新，可在既有静态路径预览。尚未推送远程。

建议先读仓库 [AGENTS.md](../AGENTS.md)，再看本文。历史同步证据见 [PR #23 验收记录](pr23-sync-validation.md)。

## 2. 当前工作区与协作方式

```bash
cd /home/bit/EV-Charging-Platform
git branch --show-current
git status --short
git log -5 --oneline --decorate
git remote -v
```

上一次检查中，以下三个文件已有用户的未提交修改，涉及 TCP 重连及对象析构，不属于本次大屏修复，不能清理或覆盖：

- `libs/core/include/charging/core/api_client.h`
- `libs/core/src/api_client.cpp`
- `tests/test_tcp_integration.cpp`

同步前新布局的恢复点仍保存在 stash，名称为 `codex-preserve-dashboard-before-pr23-20260915`；另有 `/tmp/ev-pr23-review/before-sync.patch`。先用 `git stash list` 确认，不要假设永远是 `stash@{0}`，也不要对已整合的工作区直接重复 apply。

虚拟机 fetch 曾遇到 GitHub TLS 错误。此前通过 Git bundle 校验并导入公开提交完成同步；这不是已经 push 的证明。不要强推、重置工作区，或在不了解归属时提交所有文件。

## 3. 系统边界与数据流

| 部分 | 负责什么 | 不负责什么 |
| --- | --- | --- |
| Qt 管理端、用户端与 C++ core | 登录、站点/桩、订单、计费、钱包、TCP 业务 | 不直接由 Vue 页面读 SQLite |
| SQLite 业务库 | 保存交易和业务状态 | 不等于 Spark/MySQL 分析结果库 |
| PySpark / HDFS | 导入四类业务 CSV，清洗关联，生成 DWD/DWS/ADS 分层结果 | 不把演示指标写回交易库 |
| MySQL `charging_ads` | 保存十组 ADS 结果、导入批次和元数据 | 不充当 Qt 交易主库 |
| Flask | 只读提供 `/api/v1/*` 与构建好的 `/dashboard/` 页面 | 不在请求中实时执行 Spark 作业 |
| Vue / DataV / ECharts | 展示指标、图表、错误/空状态、来源说明 | 不把缺失值伪造为 0 |
| OSM JSON 与地图底图 | 静态地理位置、站点来源信息 | 不提供实时电桩心跳、充电状态或精确设备数 |

实际分析方向：SQLite 只读导出 → 四类 CSV → HDFS → PySpark 分层计算 → 带校验清单的 ADS 导出 → MySQL 批次导入 → Flask → Vue。各阶段可能在不同虚拟机执行，Spark/MySQL 主机及目录需要与维护同学核实。

## 4. 前端维护入口

| 文件 | 主要职责 / 注意点 |
| --- | --- |
| `web/dashboard/src/App.vue` | 分组、KPI、刷新、主题、布局、来源说明；保留 AbortController 的请求归属判断，防止旧请求覆盖新数据 |
| `src/components/DashboardCard.vue` | 共享 DataV 外框；通过 ResizeObserver 测量实际卡片变化并调用 `initWH(false)`，卸载时清理；不要通过重新挂载整张卡片修尺寸，否则会重建图表 |
| `src/styles.css` | 折叠/展开、桌面浮动布局、窄屏布局；特别注意侧栏 pointer-events 和地图按钮遮挡 |
| `src/components/EChartPanel.vue` | ECharts 初始化、resize、空状态清空及销毁；散点图类型已注册 |
| `src/lib/chart-options.js` | 图表配置；排行沿用 rn；关系图用同一 TOP10 的负载和营收，不能宣称代表全部站点 |
| `src/lib/dashboard-model.js` | 响应和数字规范化；旧接口缺字段保留 null，非法数值仍拒绝；维度缺失应说明原因 |
| `src/api/analytics.js` | 读取 dashboard 与站点静态 JSON；dashboard 返回 `{ data, metadata }` |
| `src/components/MapPanel.vue` | 地图工具栏、视角、省份、站点详情弹窗 |
| `src/lib/map-view.js` | Canvas 绘制、拾取、缩放/平移、动态效果；部分旧信息栏已移除，访问可选元素必须判空 |
| `/api/v1/live/stations` | SQLite 业务站点、经纬度以及空闲/充电/故障/离线桩数量 |
| `vite.config.js` | `/dashboard/` 基础路径及开发/预览接口代理 |

本节 `src/`、`public/` 路径均相对 `web/dashboard/`。

### 外框不同步问题说明

DataV 1.7.4 的尺寸工具监听组件自身 inline style 的 MutationObserver 与 window resize。父容器通过 CSS `:hover` / `:focus-within` 动画改变尺寸时，这两种事件都可能不触发，导致 SVG 路径保持折叠尺寸。

修复放在共享 DashboardCard：观察真实根元素尺寸，以 requestAnimationFrame 合并更新，调用 DataV 的 `initWH(false)` 重算几何。这样覆盖 KPI 高度、侧栏宽度及浮动图表高度变化，保留原来的动画和图表实例。不要只给 SVG 写 `width:100%; height:100%`，那不能重算内部路径。

## 5. 数据库：在哪里看，什么尚不确定

### 5.1 SQLite 业务库

- 结构以 [database/schema.sql](../database/schema.sql) 为准，包括 users、administrators、charging_stations、charging_piles、charging_orders、recharge_records、计费规则及版本表。
- 管理端入口 `apps/admin-server/src/main.cpp` 使用**进程工作目录**下的 `charging_platform.db`。从不同目录启动会得到不同数据库；不要把测试目录的库误认为正在使用的业务库。
- 当前真实业务库的绝对路径、最近备份位置及维护责任人尚未确认。接手时先找实际运行进程的工作目录，再只读核验表、数据量和时间范围。
- `analytics/scripts/export_business_csv.py` 以只读方式导出四类业务 CSV，避免导出手机号等无关身份字段；输出目录仍应按内部数据管理。

### 5.2 MySQL 分析库

结构文件：[analytics/sql/mysql_schema.sql](../analytics/sql/mysql_schema.sql)。

| MySQL 表 | dashboard 分组 |
| --- | --- |
| ads_kpi_overview | overview |
| ads_user_level_dist | user_levels |
| ads_user_radar | user_radar |
| ads_platform_dist | platforms |
| ads_hour_trend | hour_trend |
| ads_station_type_eff | station_types |
| ads_week_compare | week_compare |
| ads_battery_health | battery_health |
| ads_area_cost | area_costs |
| ads_station_topn | top_stations |
| etl_batches / etl_metadata | 导入记录、当前批次与分析来源，不是额外业务维度 |

Flask 使用只读账号，ETL 导入使用写入账号，建表/结构调整由具备对应权限的人操作。配置变量包括 `MYSQL_HOST`、`MYSQL_PORT`、`MYSQL_USER`、`MYSQL_PASSWORD`、`MYSQL_DATABASE`、`MYSQL_POOL_SIZE`。真实配置在私有环境文件，**不要把密码、完整连接串或环境文件输出写入文档/Git**。

PR #23 包含结构和空值约束变化。`CREATE TABLE IF NOT EXISTS` 不会自动修改已经存在的旧表；需要对照旧库字段、类型、nullable 与新契约制定迁移方案，先备份再执行，不能仅“重新跑建表”就认为升级完成。

### 5.3 最近一次核验结果

- 8091 `/api/v1/health` 返回成功，但 `/api/v1/metadata` 为 404；进程尚未加载新版路由。
- `/api/v1/dashboard` 无 batch_id，排行仅有 rn、station_name、station_area；其内容与仓库 FakeRepository 测试样例一致。**这只能证明页面可接收响应，不能证明使用真实 MySQL。**
- 独立使用现有 `.env` 发起数据库只读连接，出现 OperationalError；尚未确认是网络、地址、授权还是服务配置问题。未核验 etl_metadata 是否存在、是否有有效批次。
- 既有浏览器测试使用隔离响应覆盖正常、失败与缺字段场景，测试数据不落业务库；测试通过不替代真实 Spark/MySQL 端到端核验。

## 6. 统计口径：不要沿用旧标题猜数据

事实来源：`analytics/analytics_api/metrics.py`、`analytics/spark_sql/02_dws_business.sql`、`03_ads_dashboard.sql`。

- 充电会话：charging / awaiting_payment / completed 订单。
- 已结算营收：仅 completed 的 amount + occupancy_fee；不把未完成订单金额计为收入。
- abnormal_rate：清洗、去重或关联校验剔除订单的比例，**不是 SOC 缺失率或设备故障率**。
- utilization_rate：次数 / 同组最大次数 × 100，**不是设备时间利用率**。
- 成本：批次 quality.cost_per_kwh 配置的电量成本估算；利润不含完整设备、人工等经营成本。缺 metadata 时单价显示未知，不能在前端写死。
- SOC：起始电量分布，不是电池健康诊断；缺失则空状态。
- 平台：各下单平台的去重用户，同一用户可跨平台出现。
- 雷达：等级均值 min-max 归一化；缺失或无差异维度不伪造为零。
- 周末：上海时区周六、周日，不含节假日调休规则。
- 选省只改变地图范围，分析图表仍是全平台，不能误称省级筛选结果。
- 前端 hasVerifiedSource 只检查元数据形状及脚本标识，**不是对数据库、脚本执行或数据真伪的完整认证**；最终以导出文件与接口逐项核验为准。

## 7. 接手后的实施顺序

### P0：先确认真实数据链路

1. 与数据库维护同学确认 MySQL/Spark 主机、网络、库名、账号角色和私有配置交付方式。不要使用 `.env.example` 中的示例地址当成已经验证的配置。
2. 确认现有 8091 进程是谁启动、是否测试服务、是否有人正在使用。先规划独立验证实例，再决定替换进程；不要直接杀进程或覆盖旧结果库。
3. 确认 SQLite 实际库路径和备份，确认数据时间范围与 platform/SOC 字段是否存在；缺字段是数据限制，不靠前端补造。
4. 比对 MySQL 现有结构与新契约，备份后迁移。保留旧批次、旧构建及旧入口的可恢复位置。
5. 在真实 Spark 环境生成新版结果、验证清单、导入 MySQL，再由新版 API 读取。
6. 用 verify_chain 对照全部十组值和批次。保存批次 ID、生成时间、校验输出和实际连接目标的非敏感说明。

### P1：真实结果下验收大屏

- 对照 SQL/导出与页面 KPI、排行、金额、空值；检查同一批次中的十组结果一致。
- 检查日/夜主题、KPI、两侧/浮动卡片连续展开收起，外框路径不能停在旧尺寸。
- 检查地图、省份、视角、站点弹窗、键盘操作与移动窗口尺寸；地理资料和运营指标应保持明确区分。
- 检查接口失败保留上次结果，恢复后更新；更新不应被旧请求覆盖。分析时间与最近读取时间不能混淆。

### P2：再替换旧入口

先确认“旧入口”具体指 Qt 内嵌页面、菜单链接还是浏览器地址，以及由谁维护。完成真实链路验收后，记录旧入口和旧构建，切换到验证过的新服务；有问题回到保留的旧入口。仅修改 `dist` 或打开 8091 预览不代表这一步已经完成。

## 8. 命令速查

以下命令是交接示例；带占位符的路径必须换成现场确认过的值。涉及上传、导入及服务启动的步骤会改变环境，按第 7 节的备份和协作顺序执行，不要整段盲跑。

### 前端 / 测试（BitDev）

```bash
cd /home/bit/EV-Charging-Platform/web/dashboard
# Node.js >=23；本机曾使用 ~/.nvm/versions/node/v24.21.0/bin
npm ci
npm test
npm run build
npm run dev
```

开发页面是 `http://<BitDev-IP>:5173/dashboard/`，源码修改会热更新；`ANALYTICS_PROXY_TARGET` 默认 `http://127.0.0.1:8091`。Flask 静态预览是 `http://<BitDev-IP>:8091/dashboard/`，只读取构建后的 `dist/`，修改源码后需重新 build。最近 BitDev IP 是 `192.168.202.128`，DHCP 可能改变。

```bash
# 需 Python Playwright 和 Chromium；已有浏览器可设 PLAYWRIGHT_CHROMIUM_EXECUTABLE
cd /home/bit/EV-Charging-Platform/web/dashboard
python3 tests/browser_smoke.py http://127.0.0.1:8091/dashboard/
DASHBOARD_URL=http://127.0.0.1:8091/dashboard/ python3 tests/card_resize_smoke.py

cd /home/bit/EV-Charging-Platform/analytics
PYTHONPATH=. .venv/bin/python -m unittest discover -s tests -v
```

### 数据导出 / 分析（按所在主机执行）

```bash
# 在能只读访问实际 SQLite 的主机；先设置已确认的绝对路径
cd /home/bit/EV-Charging-Platform
: "${BUSINESS_DB:?先设置实际业务库绝对路径}"
: "${CSV_DIR:?先设置独立CSV输出目录}"
python3 analytics/scripts/export_business_csv.py --db "$BUSINESS_DB" --out "$CSV_DIR"
```

```bash
# 在具备 HDFS / Spark 配置且已有 CSV 的分析主机，从仓库根目录执行
cd "${ANALYTICS_REPO_ROOT:?先设置当前主机仓库绝对路径}"  # 先设置该主机已确认的仓库绝对路径
: "${CSV_DIR:?先设置该主机上的CSV目录}"
: "${ADS_EXPORT_DIR:?先设置该主机上的ADS输出目录}"
: "${RAW_HDFS_DIR:?先确认将覆盖的HDFS原始目录}"
bash analytics/scripts/upload_raw.sh "$CSV_DIR"
spark-submit analytics/scripts/export_ads.py --analyze --out "$ADS_EXPORT_DIR"
```

```bash
# 将完整导出目录（含 manifest.json）交付给 MySQL 导入主机
cd "${ANALYTICS_REPO_ROOT:?先设置当前主机仓库绝对路径}"  # 先设置该主机已确认的仓库绝对路径
: "${ADS_EXPORT_DIR:?先设置该主机上的完整导出目录}"
python3 analytics/scripts/import_ads_mysql.py --dir "$ADS_EXPORT_DIR" --validate-only
# 先加载 ETL 专用私有环境配置，再进行真实导入
python3 analytics/scripts/import_ads_mysql.py --dir "$ADS_EXPORT_DIR"
```

分析脚本要求 Python/Spark 版本匹配（Spark 3.4.x 用 Python 3.11；3.5.x 可配 Python 3.11/3.12），使用真实 HDFS/Hive 配置。`RAW_HDFS_DIR`、`ANALYTICS_WAREHOUSE`、`COST_PER_KWH` 按现场配置确认；upload_raw.sh 会覆盖相同 HDFS 原始文件，分析运行期间不能并发替换输入。

```bash
# BitDev：先核验 analytics/.env 是只读 API 的真实配置；确认端口空闲及服务切换安排
cd /home/bit/EV-Charging-Platform
bash analytics/scripts/start_api.sh
```

更推荐先在 BitDev 的空闲本机端口 8092 验证：

```bash
cd /home/bit/EV-Charging-Platform/analytics
# 在子 shell 加载私有配置后覆盖 bind，不修改 .env 或旧进程
(
  set -a
  . ./.env
  set +a
  ANALYTICS_BIND=127.0.0.1:8092 .venv/bin/gunicorn -c deploy/gunicorn.conf.py wsgi:app
)
```

该命令在当前终端前台运行，使用另一个终端验证 `http://127.0.0.1:8092`，结束时在启动终端 Ctrl+C。Gunicorn 使用 `ANALYTICS_BIND`；`ANALYTICS_PORT` 仅用于直接运行 wsgi.py，不能拿来修改 Gunicorn 端口。要从其他主机访问独立实例，先由部署同学确认绑定地址与网络访问策略。

```bash
# 从能访问 API 且持有同批完整导出目录的主机核验
cd "${ANALYTICS_REPO_ROOT:?先设置当前主机仓库绝对路径}"  # 先设置该主机已确认的仓库绝对路径
: "${ADS_EXPORT_DIR:?先设置同批导出目录}"
: "${ANALYTICS_BASE_URL:?先设置被验收API地址，例如本机http://127.0.0.1:8092}"
python3 analytics/scripts/verify_chain.py --dir "$ADS_EXPORT_DIR" --base-url "$ANALYTICS_BASE_URL"
```

## 9. 验收与后续文档更新

历史记录（2026-09-15，整合提交 `1bbaff9`）：前端单元测试 10/10、后端接口/离线测试 21/21、Qt CTest 7/7；Qt 构建和启动检查通过。真实 Spark/MySQL 链路未完成验收。

本次外框修复（同日，基于 `1bbaff9` 的后续提交）：前端 10/10 与生产构建通过；`card_resize_smoke.py` 在日间/夜间分别连续展开收起三轮，覆盖 KPI、两侧卡片、两个浮动图表，SVG 尺寸和路径边界均与实际容器一致，无页面运行错误。该脚本比较实际几何，而不是只判断 DOM 存在。本次不涉及 Qt 或后端代码，没有将其历史测试冒充为本次重测。

后续同学完成任一阶段后，在本文补充：完成日期、提交号、执行环境、测试结果、真实批次 ID、剩余限制。不要仅写“已接通 / 测试通过”。截图、数据库、导出 CSV、node_modules、密码和机器生成日志不提交 Git。

### 尚需交接人补齐的信息

- 前端、SQLite、Spark/HDFS、MySQL、服务部署分别由谁维护及联系方式。
- 实际业务库绝对路径、备份位置和恢复步骤。
- 真实 MySQL/Spark 主机与私有配置安全交付位置。
- 首个新版真实分析批次与十组核验结果。
- 旧大屏入口位置、替换负责人和回退入口。

这些信息当前没有可靠证据，本文不代填。
