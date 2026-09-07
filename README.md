# 东软电动汽车充电桩应用管理平台

面向 Ubuntu 22.04 与 Qt 6.2 的课程项目。系统由 Qt Quick/QML 用户端、Qt Quick/QML 运营管理端、C++ 核心服务、SQLite、Web 运营大屏和独立 Python 预测服务组成。

## 项目结构

- `apps/user-client`：移动端风格用户应用，包含登录、电站、预约、充电、结算、订单、主题和地图导航。
- `apps/admin-server`：运营管理端，同时初始化数据库并启动 TCP 服务。
- `libs/core`：数据库、Repository、TCP/JSON 协议、鉴权和业务规则。
- `libs/ui`：用户端与管理端共享的 QML 主题和视觉组件。
- `database`：SQLite schema 与演示数据。
- `tests`：核心、仓储、TCP、计价、数据库维护与健壮性测试。
- `web/dashboard`：Python HTTP 服务与 ECharts 运营大屏。
- `ml`：独立训练、推理和 JSON HTTP 预测服务。
- `docs`：架构、协议、运行、测试和维护文档。

## Ubuntu 22.04 依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build \
  qt6-base-dev qt6-declarative-dev qt6-charts-dev \
  qt6-webengine-dev qt6-webengine-dev-tools \
  libqt6webenginecore6-bin libqt6sql6-sqlite \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-window \
  qml6-module-qtquick-templates qml6-module-qtqml-workerscript \
  qml6-module-qtwebengine qml6-module-qtwebengine-controlsdelegates
```

`libqt6webenginecore6-bin` 提供地图页面必需的 Qt 6 `QtWebEngineProcess`。如果点击驾车或步行导航时出现 `Could not find QtWebEngineProcess` 并闪退，先确认：

```bash
find /usr/lib/qt6 -name QtWebEngineProcess
```

不要将 Qt 5 的 `QtWebEngineProcess` 复制或软链接给 Qt 6。

## 构建与测试

唯一开发目录为 `/home/bit/charging-platform`。当前统一构建目录为 `build/admin-qml2`：

```bash
cd /home/bit/charging-platform
cmake -S . -B build/admin-qml2 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/admin-qml2 -j2
ctest --test-dir build/admin-qml2 --output-on-failure
```

截至 2026-09-07，Qt 6.2.4 全量构建通过；7 个 CTest 程序中 6 个通过。`tcp-integration-tests` 的 `reconnectAfterInitialRefusal` 用例可稳定复现失败，其余 TCP 集成场景通过。不要将这一已知失败描述为全量测试通过。

## 桌面运行

GUI 应在 VMware Ubuntu 图形桌面的终端运行。先启动管理端和 TCP 服务：

```bash
cd /home/bit/charging-platform
bash scripts/run-desktop.sh admin
```

再打开另一个桌面终端启动用户端：

```bash
cd /home/bit/charging-platform
bash scripts/run-desktop.sh user
```

脚本默认使用 `build/admin-qml2`，也可将其他构建目录作为第二个参数传入。普通 VS Code Remote SSH 终端通常没有可用的图形显示上下文，不适合人工 GUI 验收。

用户端默认连接 `127.0.0.1:45454`，可通过 `CHARGING_SERVER_HOST` 和 `CHARGING_SERVER_PORT` 覆盖。腾讯地图 Key 通过 `TENCENT_MAP_KEY` 提供，禁止写入仓库。

默认管理员账号仅用于本地开发：`admin / 123456`。首次登录后按界面要求修改密码。用户端输入合法的 11 位手机号即可登录，首次登录会自动注册。

## 其他服务

Web 大屏：

```bash
python3 web/dashboard/server.py --database charging_platform.db --host 0.0.0.0 --port 8080
```

预测服务：

```bash
python3 ml/service.py --data-dir ml/data --artifacts ml/artifacts --port 8090
```

预测服务默认地址为 `http://127.0.0.1:8090`，可通过 `CHARGING_ML_URL` 覆盖。缺少模型或数据时，管理端会明确显示演示数据或服务不可用状态。

更多说明见 [中期答辩进度说明](docs/midterm-defense-progress.md)、[用户端运行与验收](docs/user-client.md)、[QML 管理端](docs/admin-qml.md)、[手动测试指南](docs/manual-testing.md)、[协议](docs/protocol.md)和[架构](docs/architecture.md)。
