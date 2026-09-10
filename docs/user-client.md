# 用户端运行与验收

用户端采用 Qt Quick/QML 实现移动端界面。服务端、SQLite 数据库、TCP 协议和业务模型继续沿用原实现；QML 负责页面、动效与导航，`UserAppController` 负责网络请求和状态管理。

## 环境依赖

Ubuntu 22.04 / Qt 6.2 需要以下开发和 QML 运行模块：

~~~bash
sudo apt install \
  qt6-base-dev qt6-declarative-dev qt6-webengine-dev qt6-webengine-dev-tools \
  libqt6webenginecore6-bin \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-window \
  qml6-module-qtquick-templates qml6-module-qtqml-workerscript \
  qml6-module-qtwebengine qml6-module-qtwebengine-controlsdelegates
~~~

这些依赖均使用 Ubuntu 仓库中的 Qt 6.2.4，不需要升级现有 Qt 环境。`libqt6webenginecore6-bin` 提供地图页面运行所需的 `/usr/lib/qt6/libexec/QtWebEngineProcess`；只安装开发库并不一定会自动安装这个运行时包。

## 构建

在 VS Code Remote SSH 终端中执行：

~~~bash
cd /home/bit/charging-platform
cmake -S . -B build/admin-qml2 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/admin-qml2 -j2
~~~

仓库统一使用 `build/admin-qml2`，避免不同文档和脚本指向多个构建目录。

## 运行

先启动管理端和 TCP 服务：

~~~bash
cd /home/bit/charging-platform
bash scripts/run-desktop.sh admin
~~~

再打开一个 Ubuntu 图形桌面终端启动用户端：

~~~bash
cd /home/bit/charging-platform
export CHARGING_SERVER_HOST=127.0.0.1
export CHARGING_SERVER_PORT=45454
export TENCENT_MAP_KEY=你的腾讯地图Key
bash scripts/run-desktop.sh user
~~~

`TENCENT_MAP_KEY` 不写入仓库。腾讯 URI 路线规划在部分环境下可以不传 Key；建议演示时配置已授权的 Key。

## 代码结构

- `qml/Main.qml`：应用窗口、页面路由、顶部栏、底部导航和全局反馈。
- `qml/Theme.qml`：全局颜色、间距、圆角和字体规格。
- `qml/components/`：按钮、卡片、状态徽标、站点卡片和底部导航。
- `qml/pages/`：登录、首页、电站详情、充电、个人中心和地图页面。
- `src/user_app_controller.*`：登录、站点、电桩、预约、充电、订单、钱包、定位和地图业务状态。

## 演示账号

| 场景 | 手机号 | 预置状态 |
|---|---|---|
| 有余额 | 18800000001 | 余额 200 元，可完成预约充电 |
| 待结算 | 18800000002 | 有一笔 9.45 元待结算订单 |
| 低余额 | 18800000003 | 余额 0.50 元 |
| 已冻结 | 18800000004 | 登录会被服务端拒绝 |

演示数据采用 `INSERT OR IGNORE`，不会覆盖这些账号后续产生的钱包和订单变化。

## 页面与功能

- 登录：11 位手机号免密登录、首次登录自动注册、演示账号快捷填入。
  同一账号只允许一个客户端在线；在另一台设备登录后，本设备会被服务端接管下线，
  自动退回登录页并提示“账号已在其他设备登录，当前设备已下线”。
- 首页：模拟定位、地址搜索、按距离排序的现代化充电站卡片。
- 电站详情：站点指标、电桩状态、预约、驾车与步行导航。
- 充电：活动订单、开始、实时计时、停止、取消和钱包结算。
- 我的：资料编辑、余额充值、历史订单和退出登录。
- 地图：通过 QML `WebEngineView` 打开腾讯地图路线规划。

## 定位说明

当前为课程项目的模拟定位，支持深圳、北京、沈阳三个城市中心点；其他地址默认以深圳市中心为当前位置。站点距离使用经纬度和 Haversine 公式在本地计算。

## 验收流程

1. 启动管理端和用户端，使用有余额账号登录。
2. 确认首页站点按距离显示，搜索和定位可刷新列表。
3. 进入深圳演示充电站，预约闲置电桩 `SZ001-01`。
4. 切换“充电”标签，确认开始后电桩状态与计时更新，再停止并结算。
5. 切换“我的”，确认余额和订单历史同步更新。
6. 分别点击驾车、步行导航，确认地图接收当前位置与目标坐标。
7. 使用冻结账号登录，确认服务端拒绝登录且页面显示错误反馈。
8. 开启第二个用户端，用同一手机号登录；确认第二个客户端登录成功，
   而第一个客户端在约 1 秒内退回登录页并提示已在其他设备登录；
   在第一个客户端重新登录，应能反向接管第二个客户端。

## 自动验证

~~~bash
ctest --test-dir build/admin-qml2 --output-on-failure
~~~

在无图形桌面的环境中可做启动冒烟测试：

~~~bash
timeout 12s env \
  QT_QPA_PLATFORM=offscreen \
  QT_QUICK_BACKEND=software \
  QTWEBENGINE_DISABLE_SANDBOX=1 \
  ./build/admin-qml2/apps/user-client/charging-user
~~~

进程持续运行到 `timeout`（退出码 124）且没有 QML 报错，即表示应用窗口和所有声明式页面成功加载。

## 地图与 WebEngine 排错

开头出现 `Warning: Ignoring WAYLAND_DISPLAY on Gnome` 通常只是 Qt 选择 X11/xcb 后端的提示，不是地图闪退原因。

如果点击“驾车导航”或“步行导航”后出现以下日志并退出：

~~~text
Could not find QtWebEngineProcess
已中止 (核心已转储)
~~~

安装缺失的 Qt 6 WebEngine 二进制包并重新启动：

~~~bash
sudo apt install libqt6webenginecore6-bin
find /usr/lib/qt6 -name QtWebEngineProcess
~~~

预期路径为 `/usr/lib/qt6/libexec/QtWebEngineProcess`。系统中的 `/usr/lib/x86_64-linux-gnu/qt5/libexec/QtWebEngineProcess` 属于 Qt 5，不能用于当前 Qt 6 应用。
