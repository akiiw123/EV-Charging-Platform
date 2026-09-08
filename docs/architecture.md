# 系统架构与代码边界

本文描述最终答辩版本的真实实现。阅读顺序建议从本文件开始，再看 `code-reading-guide.md` 和具体源码顶部的职责说明。

## 1. 总体架构

```text
Qt 用户端（QML） ─┐
                   ├─ TCP / 一行一条 JSON ─ Qt 管理端进程内 TCP 服务 ─ Repository ─ SQLite
Qt 管理端（QML） ─┘                         │
                                           └─ HTTP JSON ─ Python 预测服务

Web 运营大屏 ─ HTTP JSON ─ Python 只读统计服务 ─ SQLite
```

管理端可执行程序同时承担两个角色：显示运营界面，并初始化数据库和启动 TCP 服务。用户端不直接访问数据库，只通过 `ApiClient` 发请求。预测模型在独立 Python 进程运行，避免将 Python 和耗时推理嵌入 GUI 线程。

## 2. 分层职责

| 层 | 目录 | 负责 | 不负责 |
|---|---|---|---|
| 展示层 | `apps/*/qml` | 布局、主题、轻量动画、输入和操作入口 | SQL、阻塞网络、业务状态迁移 |
| ViewModel | `apps/*/src/*controller*` | 页面状态、校验、筛选、异步请求、响应落模 | 直接拼业务 SQL |
| 通信层 | `api_client.*`、`tcp_server.*`、`message_protocol.*` | 连接、重连、帧边界、请求 ID、线程调度 | 决定订单规则 |
| 服务编排 | `request_router.*`、`session_registry.*` | 鉴权、接口路由、单会话、参数和状态约束 | 绘制页面 |
| 数据访问 | `repositories.*`、`database_manager.*` | SQL、事务、连接和数据库初始化 | 处理 QML 状态 |
| 公共规则 | `business_rules.h`、`display_time.h`、`password_security.*` | 状态常量、输入边界、时间和密码安全 | 保存界面状态 |
| 预测 | `ml/` | 数据导出、训练、模型加载和 HTTP 推理 | 修改订单、电桩或用户数据 |
| 运营大屏 | `web/dashboard` | SQLite 只读统计和浏览器图表 | 写业务数据 |

## 3. 一次用户操作如何流动

以“预约电桩”为例：

1. `StationDetailPage.qml` 收集点击事件，调用 `UserAppController::reserve()`。
2. 控制器通过 `ApiClient` 发送带请求 ID 的 `order.reserve` JSON。
3. `TcpServer` 在线程任务中读取一行消息并交给 `RequestRouter`。
4. 路由器验证登录会话和参数，再调用 `OrderRepository::createReservation()`。
5. Repository 在事务中检查用户和电桩的活动订单唯一约束并写入 SQLite。
6. 响应沿原连接返回；控制器更新活动订单和站点模型，QML 绑定自动刷新。

这个链路体现项目的核心边界：QML 不知道 SQL，Repository 不知道页面，协议层用状态值而界面负责中文显示。

## 4. 进程、端口与数据

- 管理端/TCP 服务：默认 `0.0.0.0:45454`。
- 用户端：默认连接 `127.0.0.1:45454`，可用 `CHARGING_SERVER_HOST`、`CHARGING_SERVER_PORT` 覆盖。
- 预测服务：默认 `127.0.0.1:8090`，管理端可用 `CHARGING_ML_URL` 覆盖。
- Web 大屏：演示命令默认监听 `0.0.0.0:8080`。
- SQLite：schema 事实来源为 `database/schema.sql`，运行库默认在项目根目录生成且不提交 Git。
- 腾讯地图：Key 仅从 `TENCENT_MAP_KEY` 读取。

## 5. 并发、会话与安全

- 每个 TCP 连接使用独立数据库连接，避免跨线程共享 `QSqlDatabase`。
- 服务端限制最大并发连接数；容量耗尽返回 `SERVER_BUSY`，不静默排队。
- `SessionRegistry` 保证同一用户账号仅保留一个在线客户端；后登录连接接管旧连接。
- 用户身份绑定连接，业务请求不能由客户端任意指定用户 ID。
- 订单状态变更和钱包结算使用事务；数据库唯一索引阻止同一用户或电桩出现多个活动订单。
- 管理员密码使用 PBKDF2-SHA256；默认开发密码首登后要求修改。
- 协议错误对客户端返回稳定错误码，底层数据库细节只进入服务端日志。

## 6. 当前真实限制

- 位置是课程演示定位和地址地理编码组合，不等同于移动设备 GPS。
- Web 大屏服务为局域网演示用途，直接只读 SQLite，正式部署仍需鉴权、HTTPS 和统一统计服务。
- 分时电价已有仓储和计算能力；既有订单结算仍保留固定电价口径，占位费尚未接入结算。
- 数据库没有可靠的电桩心跳时间和电站更新时间字段，界面不伪造这些值。
- ML 服务必须存在训练数据与模型产物；不可用时管理端明确显示演示数据或失败状态。
- `apps/` 根目录下的旧 QML/Widgets 原型不参与当前根 CMake 构建；实际入口只在 `apps/user-client` 和 `apps/admin-server`。
