# QML 运营管理端

管理端已由 Qt Widgets 重构为 Qt Quick/QML 桌面应用，主要适配 1440×900，最低窗口尺寸为 1280×720。数据库、TCP/JSON 协议、密码校验及用户端业务逻辑保持兼容。

## 设计系统

共享模块位于 `libs/ui`，模块 URI 为 `Charging.UI`。管理端统一使用语义令牌，不在页面中根据主题逐项判断颜色。

- 默认品牌色：`#0F9F8F`，与用户端青绿品牌保持一致。
- 默认背景/表面：`#F4F7FA` / `#FFFFFF`。
- 深海主题背景/表面/强调色：`#07111F` / `#102039` / `#18C8F4`。
- 字号：11 / 13 / 15 / 20 / 28 px，并支持 85%～130% 缩放。
- 间距：4 px 起步的 8 px 主栅格；页面边距 24 px，模块间距 12～18 px。
- 圆角：8 / 12 / 16 px；卡片为 1 px 语义边框。
- 动画：140 ms 控件反馈、200 ms 页面与抽屉过渡，使用 OutCubic，无弹跳和持续闪烁。
- 图标：统一使用 `LineIcon` Canvas 线性图标，不混用 Emoji 或位图。
- 状态：成功、品牌、信息、警告、危险和离线均同时显示文字，不仅依赖颜色。

主题共七套：默认青绿、深海青蓝、极光紫、石墨橙、翡翠绿、云白蓝、高对比。主题、侧栏状态、动画、字体缩放和表格页大小通过 `QSettings` 持久化。

## 代码结构

- `libs/ui/qml/Theme.qml`：共享语义令牌与七套主题。
- `libs/ui/qml/`：共享卡片、按钮、输入框、状态徽标和线性图标。
- `apps/admin-server/qml/components/`：桌面壳层、侧栏、顶栏、数据表、弹窗和详情抽屉。
- `apps/admin-server/qml/pages/`：登录、总览、电站、电桩、订单、用户、预测和设置。
- `apps/admin-server/src/admin_app_controller.*`：页面状态、异步 TCP/HTTP 请求、筛选和设置。
- `apps/admin-server/src/json_list_model.h`：面向 QML 代理复用的数据列表模型。
- `libs/core/src/request_router.cpp`：已有管理协议及新增订单列表、电站编辑/删除接口。

页面层不执行 SQL。数据库操作仍由 TCP 服务端路由完成；机器学习推理由独立 Python HTTP 服务执行，不阻塞 GUI 线程。

## 构建

```bash
cd /home/bit/charging-platform
cmake -S . -B build/admin-qml2 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/admin-qml2 -j2
```

## 启动

在 VMware 的 Ubuntu 图形桌面终端运行：

```bash
cd /home/bit/charging-platform
./build/admin-qml2/apps/admin-server/charging-admin
```

管理端进程会同时初始化 SQLite 数据库并启动 TCP 服务。随后可在另一个终端启动用户端：

```bash
./build/admin-qml2/apps/user-client/charging-user
```

VS Code Remote SSH 终端通常没有图形显示连接。如果 `/tmp/.X11-unix/X0` 存在，可先设置 `DISPLAY=:0`；否则应在 VMware 桌面终端启动，不要使用 `offscreen` 做人工演示。

## 智能预测

预测服务由队友的 `ml/service.py` 提供：

```bash
cd /home/bit/charging-platform/ml
python3 service.py --data-dir ./data --artifacts ./artifacts --port 8090
```

管理端默认访问 `http://127.0.0.1:8090`，可通过 `CHARGING_ML_URL` 覆盖。服务或模型未准备好时，预测页明确标记为“演示数据”，不会将模拟值冒充模型结果。

## 验证

```bash
ctest --test-dir build/admin-qml2 --output-on-failure
```

无图形环境可验证 QML 加载：

```bash
timeout 12s env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/admin-qml2/apps/admin-server/charging-admin
```

退出码 124 且没有 QML 日志表示窗口持续运行并成功加载。

## 当前限制

- 数据库尚无“电桩最近心跳”字段，表格明确显示“暂无心跳字段”；未伪造心跳时间。
- 当前电站删除受历史订单外键保护；包含关联订单的电站会被服务端拒绝删除。
- 机器学习服务需要训练产物和数据目录。服务不可用时仅展示带标识的集中演示数据。
- 表格已使用 C++ 模型、搜索、状态筛选和滚动；分页设置已持久化，但服务端分页协议尚未实现，当前接口最多返回 200～500 条。
- 电站详情展示已有汇总字段；数据库暂无电站独立“更新时间”列，继续显示创建时间，未伪造更新时间。
