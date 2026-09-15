# PR #23 同步与大屏验收记录

日期：2026-09-15。开发目录：`/home/bit/EV-Charging-Platform`，分支：`glm`。

## 同步结果

- 从 PR #22 的 `3e838ef` 快进到 PR #23 合并提交 `4c65d2c8437bf96e192bf0fbf5e510b3e2f52ede`，完整保留 PR #23 的 35 个文件更新。
- 同步前用户的 12 个未提交文件已保存在 `stash`，名称 `codex-preserve-dashboard-before-pr23-20260915`，另有 `/tmp/ev-pr23-review/before-sync.patch`。
- 逐段解决 App.vue、chart-options.js、dashboard-model.test.mjs 三个冲突。中央沉浸地图、折叠 KPI、两侧展开卡片、分组按钮、左下排行与右上关系图的位置和主题均保留。
- 原有 ApiClient 和 TCP 测试修改与同步前备份完全一致，未纳入本次大屏提交。
- 虚拟机直接 fetch 连续出现 TLS 错误；通过 Windows 获取公开仓库对象，Git bundle 校验通过后导入虚拟机的 origin/main 跟踪引用，再进行快进。没有改写公共历史，也未推送远程。

## 修复内容

1. 保留 PR #23 的并发刷新保护、metadata、空值及缺失维度提示。
2. 新布局已移除地图信息栏与动态按钮，地图逻辑改为仅在元素存在时更新，修复初始化异常与站点不绘制。
3. 补齐站点负载与营收散点图，使用同组 TOP10 的真实字段；缺值不绘制为零。排行保持按 rn 展示充电次数。
4. 旧接口行字段缺失时保留 null 和明确空状态，不再让单一维度缺字段阻断全屏；非法数字仍报错。
5. 修复侧栏间隙鼠标事件穿透，以及 2D/2.5D 按钮被右侧浮动卡片遮挡。
6. 增加侧栏、KPI 与浮动卡片的键盘聚焦展开；修正窄屏双图容器高度，避免后续卡片重叠。
7. 同步结算营收、订单剔除比例和批次成本单价说明。来源信息放在现有底部折叠说明内，避免挤压中央布局。

## 验证结果

| 检查 | 结果 |
| --- | --- |
| 前端单元测试 | 10/10 通过 |
| Vite 生产构建 | 通过；仍有超过 500 kB 的 bundle 体积提示 |
| Python 接口与离线处理测试 | 21/21 通过；离线 SQL 使用 SQLite 适配器，不等价于真实 Spark/MySQL 验证 |
| Qt 全量构建 | 通过 |
| CTest | 7/7 通过，含核心、仓储、TCP、计费、数据库与时间测试 |
| Qt 管理端、用户端无界面启动 | 均持续运行至 12 秒超时（退出码 124）；无 QML 加载失败，用户端有既存 Qt WebEngine 类型注册警告 |
| 浏览器地图 | 3460 个 OSM 静态站点；省份/返回全国、2D/2.5D、缩放/复位通过 |
| 站点交互 | 实际站点点击、详情、关闭与键盘缩放/平移/返回通过 |
| 页面交互 | 图表分组、侧栏、浮动图表、日夜主题及主题记忆通过 |
| 故障与空数据 | 浏览器隔离测试响应：503 保留上次数据、重试恢复、缺 SOC/platform 提示通过 |
| 响应式 | 1920×1080、1440×900、1280×720、1024×768、390×844 无横向溢出；手机地图滚入视口后绘制、省份选择通过 |
| JavaScript 运行错误 | 最终浏览器回归中为 0 |
| Git 格式检查 | `git diff --check` 通过 |

浏览器测试数据仅通过页面请求拦截注入，不导入 MySQL/SQLite，也不作为项目业务数据。截图中的测试批次为 `browser-test-only`。

## 替换旧版前仍需完成

**当前不能宣称新版真实数据链路已验收。**

- 现有 `8091` 进程仍没有 PR #23 的 `/api/v1/metadata` 路由（返回 404），dashboard 响应没有 batch_id。
- 当前站点排行只有 rn、station_name、station_area，缺次数、营收和负载字段；响应与仓库 FakeRepository 测试样例一致，不能据此证明实际 MySQL 数据已经接通。
- 在读取现有私有 `.env` 的独立只读连接中发生 OperationalError，尚未确认新版 etl_metadata 表和有效分析批次。
- 本次没有迁移或重写业务/分析数据库，没有替换运行中的 Flask 进程，没有把 Qt 旧大屏入口切到新页面。
- 当前构建的前端可由既有 Flask 静态页面路径预览；这不等于已切换真实分析后端。

下一步应先确认真实 MySQL 连接，按 PR #23 的分析流程导入一批新版 ADS，启动新版 API，并用 `analytics/scripts/verify_chain.py` 核对批次与十组结果；核验后再切换旧入口。不得用测试数据代替上述步骤。

## 复验命令

```bash
cd /home/bit/EV-Charging-Platform/web/dashboard
npm test
npm run build
python3 tests/browser_smoke.py http://127.0.0.1:8091/dashboard/

cd /home/bit/EV-Charging-Platform/analytics
PYTHONPATH=. .venv/bin/python -m unittest discover -s tests -v
```

浏览器测试需 Playwright 与 Chromium；可通过 `PLAYWRIGHT_CHROMIUM_EXECUTABLE` 指定现有浏览器。`--output` 指定截图/结果目录，默认使用临时目录。本次证据保存在 `/tmp/ev-pr23-review/`，不提交生成截图或数据库。
