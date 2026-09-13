# 最终答辩指南

## 1. 项目一句话介绍

这是一个以 Qt Quick/QML 为双端界面、C++ TCP 服务与 SQLite 为业务核心，并接入 Web 运营大屏和独立 Python 负荷预测服务的电动汽车充电运营平台。

## 2. 建议五人分工

| 成员 | 主讲内容 | 必看代码 |
|---|---|---|
| 1 | 总体架构、TCP/JSON、会话与安全 | `architecture.md`、`message_protocol.*`、`tcp_server.*`、`session_registry.*` |
| 2 | 用户端和预约充电闭环 | `user_app_controller.*`、用户端 `Main.qml`、`ChargingPage.qml` |
| 3 | 管理端和设备运营 | `admin_app_controller.*`、`DashboardPage.qml`、`StationsPage.qml`、`PilesPage.qml` |
| 4 | 数据、Web 大屏与 ML | `schema.sql`、`web/dashboard`、`ml/data.py`、`model.py`、`service.py` |
| 5 | Repository、测试与质量保障 | `repositories.*`、`database_maintenance.*`、`tests/` |

每位成员都应先通读 `architecture.md`，并能完整说明“QML → Controller → TCP → Router → Repository → SQLite → 响应返回”的链路。

## 3. 十分钟演示顺序

1. 展示架构图和模块目录（1 分钟）。
2. 管理端登录，展示总览、主题、电站和电桩状态（1.5 分钟）。
3. 用户端手机号登录、定位和站点筛选（1 分钟）。
4. 预约 → 开始充电 → 停止 → 结算 → 历史订单（3 分钟）。
5. 管理端观察数据同步，演示冻结/设备约束或单账号会话接管（1 分钟）。
6. 地图导航、Web 大屏和预测页面（1.5 分钟）。
7. 展示自动测试与真实限制（1 分钟）。

## 4. 答辩前启动

```bash
cd /home/bit/EV-Charging-Platform
cmake -S . -B build/admin-qml2 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/admin-qml2 -j2
ctest --test-dir build/admin-qml2 --output-on-failure
```

Ubuntu 图形桌面第一个终端：

```bash
bash scripts/run-desktop.sh admin
```

第二个终端：

```bash
export TENCENT_MAP_KEY=答辩现场有效Key
bash scripts/run-desktop.sh user
```

可选预测服务：

```bash
python3 ml/service.py --data-dir ml/data --artifacts ml/artifacts --port 8090
```

可选 Web 大屏：

```bash
python3 web/dashboard/server.py --database charging_platform.db --host 0.0.0.0 --port 8080
```

## 5. 演示前检查清单

- `find /usr/lib/qt6 -name QtWebEngineProcess` 能找到 Qt 6 辅助进程。
- 管理端先启动，用户端显示已连接。
- 地图 Key 不出现在代码、日志、截图和提交中。
- 演示账号余额和活动订单状态符合预期。
- 测试结果如有失败，按真实结果说明，不宣称全绿。
- ML 无模型时主动说明“演示数据/服务不可用”，不伪装真实预测。
- 不现场删除数据库或绕过业务流程修数据。

## 6. 核心技术问答口径

### 为什么采用 TCP + JSON？

课程项目需要展示网络编程和跨端业务。TCP 保证有序可靠传输，JSON 便于调试和扩展；项目用换行符解决 TCP 粘包/拆包边界，并用请求 ID 关联响应。

### 如何保证并发预约正确？

服务端先做业务校验，Repository 使用事务更新；数据库还有“每个用户/电桩只能有一个活动订单”的部分唯一索引作为最终保护。并发测试验证只有一个请求成功。

### 如何保证客户端不能冒充别人？

登录后用户 ID 绑定在该 TCP 连接的 `RequestRouter` 会话中。订单、钱包和资料接口使用服务端保存的身份，不相信客户端提交的任意用户 ID。

### 为什么页面不会因查询而卡死？

GUI 只发异步 TCP/HTTP 请求，响应通过 Qt 信号进入 Controller，再更新模型；SQL 和 ML 推理不在 QML 或 GUI 线程同步执行。

### 机器学习是不是假数据？

仓库有完整的数据加载、脱敏导出、PyTorch 训练、产物加载和 HTTP 推理代码。模型/数据没有启动时，界面会明确标注演示数据或错误，这与真实推理状态严格区分。

### 项目还有哪些限制？

当前定位不等同于移动设备 GPS；Web 大屏是局域网只读演示服务；服务端分页、TLS、正式部署鉴权和监控仍需工程化；分时电价和占位费尚未完全接入订单结算。

## 7. 最后熟悉代码的方法

每位成员选择自己的模块，按源码顶部职责说明逐个过文件；对每个公开方法至少能回答三件事：谁调用、输入是什么、状态最终保存在哪里。不要背代码，重点掌握数据流、状态机和错误分支。
