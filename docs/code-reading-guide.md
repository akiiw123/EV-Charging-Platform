# 代码阅读与答辩导览

## 1. 推荐阅读路线

1. `CMakeLists.txt`：确认实际构建模块，理解哪些目录是有效入口。
2. `database/schema.sql`：先认识用户、电站、电桩、订单和计价表及约束。
3. `libs/core/include/charging/core/models.h`：理解数据库记录在 C++ 中的表示。
4. `message_protocol.*` → `api_client.*` → `tcp_server.*`：理解 TCP/JSON 数据怎么走。
5. `request_router.*` → `repositories.*`：理解业务校验、事务和状态迁移。
6. 两端 `*AppController`：理解后端数据如何转换成页面状态。
7. 两端 `qml/Main.qml` 和 `qml/pages/`：理解页面导航及用户操作入口。
8. `ml/`、`web/dashboard/`：最后看独立预测服务和运营大屏。

每个项目自有源码顶部都有职责说明，先读这两行，再进入实现。第三方 `web/dashboard/echarts.min.js` 不修改、不作为答辩源码讲解。

## 2. 核心层逐文件索引

| 文件 | 一句话说明 |
|---|---|
| `database_manager.*` | 创建 SQLite 连接、执行 schema/seed 和管理连接生命周期 |
| `models.h` | 用户、管理员、电站、电桩、订单、计价规则的数据结构 |
| `repositories.*` | 所有核心 SQL、事务及数据实体读写 |
| `business_rules.h` | 状态、金额、手机号等可复用业务规则 |
| `message_protocol.*` | JSON 消息编解码、请求 ID、成功/错误信封 |
| `api_client.*` | 两个 GUI 共用的异步客户端和断线重连 |
| `tcp_server.*` | 监听、连接容量、线程任务和会话关闭通知 |
| `request_router.*` | 协议类型分派、鉴权、参数校验和业务编排 |
| `session_registry.*` | 同一车主账号只保留一个在线会话 |
| `password_security.*` | PBKDF2 密码哈希与验证 |
| `display_time.h` | SQLite UTC 时间到 UTC+8 展示时间的统一转换 |
| `database_maintenance.*` | 数据库检查、备份、恢复和 WAL 边车处理 |

## 3. 用户端

- `src/main.cpp`：初始化 Qt WebEngine、注册资源、暴露控制器并加载 `Main.qml`。
- `src/user_app_controller.*`：用户端唯一主要 ViewModel；重点看 `handleResponse()`、`rebuildStations()`、订单动作、定位和 `openNavigation()`。
- `qml/Main.qml`：登录态、页面栈、顶部栏和底部导航总装。
- `qml/pages/LoginPage.qml`：手机号校验和快捷演示账号。
- `HomePage.qml`：位置、搜索、筛选和距离排序后的站点列表。
- `StationDetailPage.qml`：电桩列表、预约和地图入口。
- `ChargingPage.qml`：活动订单状态机的界面表达。
- `OrdersPage.qml`、`ProfilePage.qml`：历史、资料、钱包、头像和主题。
- `MapPage.qml`：仅承载控制器生成的腾讯地图 URL；WebEngine 辅助进程由系统包提供。
- `qml/components/`：页面复用的小组件，不包含业务 SQL。

## 4. 管理端

- `src/main.cpp`：初始化 SQLite、TCP 服务、控制器和 QML。
- `src/admin_app_controller.*`：管理页面数据、客户端筛选、TCP 请求、ML HTTP 请求和设置持久化。
- `src/json_list_model.h`：把 JSON 数组暴露成 QML 可复用模型，表格不硬编码静态行。
- `qml/Main.qml`、`components/AppShell.qml`：登录态和宽屏管理框架。
- `DashboardPage.qml`：营收、状态、站点能耗和趋势范围。
- `StationsPage.qml`、`PilesPage.qml`：设备资料与危险操作确认。
- `OrdersPage.qml`、`UsersPage.qml`：订单检索、用户脱敏和冻结操作。
- `PredictionPage.qml`：展示真实预测或明确标识的演示/失败状态。
- `SettingsPage.qml`：七套主题和显示偏好。

## 5. 数据库与订单状态机

```text
reserved ──开始──> charging ──停止──> awaiting_payment ──扣款──> completed
    └─取消/超时──────────────────────────────────────────────> cancelled
```

电桩状态使用 `idle / charging / fault / offline`，用户状态使用 `active / frozen`。数据库存英文稳定值，QML 映射中文，避免协议和持久化受展示文案变化影响。

答辩时重点说明三个保护：活动订单唯一索引、事务回滚、服务端鉴权。即使两个客户端同时预约同一电桩，最终也只能有一个成功。

## 6. ML 与大屏

- `ml/export.py`：从 SQLite 只读导出“站点 × 小时”脱敏数据。
- `ml/data.py`：加载 UrbanEV/自有同结构数据并构造时间、天气、价格特征。
- `ml/model.py`：PyTorch 负荷预测模型。
- `ml/train.py`：训练、验证、早停并保存模型与元数据。
- `ml/service.py`：加载产物，提供 `/health`、`/stations`、`/predict`。
- `web/dashboard/server.py`：只读统计 API 和静态文件服务。
- `web/dashboard/app.js`：每 5 秒更新指标与 ECharts。

## 7. 测试如何对应功能

| 测试 | 重点 |
|---|---|
| `test_message_protocol.cpp` | 协议、请求 ID、错误与密码安全 |
| `test_repositories.cpp` | CRUD、事务和订单主流程 |
| `test_tcp_integration.cpp` | 真实 TCP、鉴权、并发、多客户端、单会话和管理接口 |
| `test_pricing.cpp` | 分时价格、占位费计算和固定价回归 |
| `test_database_maintenance.cpp` | 备份、恢复和损坏拒绝 |
| `test_database_robustness.cpp` | 错误数据、保存失败、并发竞争和较大数据量 |
| `test_display_time.cpp` | UTC、偏移和跨日显示 |

## 8. 容易被问到的问题

- 为什么不用 QML 直接查数据库？为了隔离展示与业务、避免阻塞 GUI，并让服务端统一鉴权和事务。
- 为什么每个连接独立数据库连接？`QSqlDatabase` 连接不能跨线程安全共享。
- 为什么 ML 是 HTTP 服务？Python 依赖和推理解耦，失败时 GUI 仍可运行。
- 为什么大屏直接读 SQLite？课程局域网演示的简化；正式系统应走受鉴权的统计服务。
- 为什么有 `apps/` 根目录旧代码？它是迁移前原型，根 CMake 没有引用；答辩以两个子应用目录为准。
