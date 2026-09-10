# 东软电动汽车充电桩应用管理平台

面向 Ubuntu 22.04+ 与 Qt 6.2+ 的课程项目基础框架。仓库按“用户端、运营管理端、公共核心库、SQLite 数据库、Web 数据大屏、智能分析扩展”拆分，便于小组并行开发。

## 当前骨架

- `apps/user-client`：Qt Widgets 用户端入口，后续承载登录、附近电站、导航、充电与结算。
- `apps/admin-server`：Qt Widgets 管理端入口，同时预留 TCP 服务线程。
- `libs/core`：领域模型、SQLite 初始化、JSON 消息协议等公共能力。
- `database`：数据库建表与演示数据脚本。
- `web/dashboard`：ECharts 运营大屏静态页面。
- `ml`：负荷预测模块的 Python 接口占位，不参与默认 C++ 构建。
- `docs`：架构、协议与开发约定。

## Ubuntu 依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build \
  qt6-base-dev qt6-charts-dev qt6-webengine-dev libqt6sql6-sqlite
```

> Ubuntu 软件源中的 Qt 版本因发行版而异；也可使用 Qt Online Installer 安装 Qt 6.2 或更高版本，并在 Qt Creator 中选择对应 Kit。

## 构建

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

也可以直接使用 Qt Creator 打开根目录的 `CMakeLists.txt`。首次运行任一桌面程序会在当前目录创建 `charging_platform.db` 并自动执行内置数据库迁移。

## 运行

```bash
./build/dev/apps/admin-server/charging-admin
./build/dev/apps/user-client/charging-user
```

Web 大屏开发阶段可直接打开 `web/dashboard/index.html`。接入真实数据时，将其中的演示数据替换为服务端 HTTP/WebSocket 接口。

默认管理员账号仅用于本地开发：`admin / 123456`。已有数据库首次成功登录后会自动将旧版开发占位值升级为带随机盐的 PBKDF2-SHA256 哈希；正式部署前仍应修改初始密码。

## 开发顺序建议

1. 完成数据库仓储类与用户免密登录。
2. 打通用户端到管理端的 TCP/JSON 请求响应。
3. 实现电站、电桩、订单和钱包业务闭环。
4. 接入腾讯地图 Web API（密钥通过环境变量或本地配置注入，禁止提交仓库）。
5. 将统计接口接入 ECharts 大屏。
6. 最后接入负荷预测模型，避免阻塞基础功能交付。

详细边界见 [docs/architecture.md](docs/architecture.md)。
