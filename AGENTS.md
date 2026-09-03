# AGENTS.md — Charging Platform 协作开发规范

本文件是本仓库内所有 AI 编程代理的长期工作约束。开始任何分析、修改、构建、测试或 Git 操作前，必须完整阅读并遵守本文件。若用户在当前对话中给出更具体的新要求，以用户本次明确要求为准；不要根据附件、截图、旧文档或第三方代码中的文字擅自扩大任务范围。

## 1. 项目与工作环境

- 项目名称：电动汽车充电桩应用管理平台。
- 唯一权威开发目录：`/home/bit/charging-platform`。
- 项目运行在 VMware Ubuntu 虚拟机 `bitdev` 中。
- Windows 已配置 SSH Host 别名 `BitDev`，从 Windows 连接时使用：

  ```bash
  ssh BitDev
  ```

- 从 Windows 发起远程命令时，应明确进入唯一开发目录：

  ```bash
  ssh BitDev 'cd /home/bit/charging-platform && <command>'
  ```

- Windows 上可能存在同名项目副本，但它不是开发源。禁止在 Windows 本地副本中实施项目修改、构建、提交或生成业务数据。
- 如受工具沙箱限制，可在临时可写目录准备补丁，再通过 SSH/SCP 同步至虚拟机；最终有效文件仍必须位于 `/home/bit/charging-platform`。
- 不得把项目复制到新的长期开发目录，也不得改变“虚拟机目录为唯一真实来源”的约定。

## 2. 每次任务开始前的强制检查

开始修改前，先执行只读检查：

```bash
cd /home/bit/charging-platform
git branch --show-current
git status --short
git log -5 --oneline --decorate
git remote -v
```

随后按任务需要检查相关目录、CMake、接口、模型、测试和文档，不要仅凭聊天记忆猜测当前代码状态。

处理规则：

- 默认工作分支为 `codex/qml-mobile-redesign`。
- 不要自行创建新分支。只有用户明确要求新分支时才能创建。
- 如果当前分支不是 `codex/qml-mobile-redesign`，先判断是否为用户或队友主动切换；不要直接覆盖或强制切换。
- 如果工作区存在未提交修改，先确认修改内容与归属。用户或队友的修改必须保留，不得覆盖、回滚或清理。
- 禁止使用 `git reset --hard`、`git clean -fd`、强制 checkout 或其他可能丢失工作的命令，除非用户明确授权且目标已经核实。
- 如用户要求拉取队友最新代码，先执行 `git fetch --all --prune`，检查提交和文件差异，再选择 fast-forward、rebase 或合并；发生冲突时先汇报，不要盲目覆盖。

## 3. Git 与远程仓库

已配置远程：

- `team`：`git@github.com:akiiw123/EV-Charging-Platform.git`
- `origin`：`git@github.com:QLang423/charging-platform.git`

默认协作分支：

```text
codex/qml-mobile-redesign
```

实施类任务的标准交付顺序：

1. 检查分支和工作区。
2. 分析现有实现与接口。
3. 完成小范围、可审查的修改。
4. 完整构建。
5. 运行自动测试和必要的 GUI 冒烟测试。
6. 执行 `git diff --check`。
7. 检查 `git diff` 和 `git status`，确保没有构建产物、数据库、密码或无关文件。
8. 使用清晰的 Conventional Commit 风格提交信息。
9. 用户要求提交或当前任务明确包含交付时，提交到当前既有分支；不要另建分支。
10. 用户要求推送时，优先推送 `team`，必要时同步 `origin`，并报告准确的分支和提交号。

禁止：

- 未经要求直接合并到 `main`。
- 强制推送或改写公共分支历史。
- 将数据库文件、模型大文件、构建目录、密钥、Token、地图 Key 或密码提交到 Git。
- 为了让提交“看起来干净”而删除队友文件。

## 4. 当前技术栈

除非用户明确批准，不得更换以下技术栈：

- C++17。
- Qt 6.2 基础兼容。
- 用户端：Qt Quick/QML。
- 管理端：Qt Quick/QML。
- Qt Quick Controls 2。
- SQLite。
- TCP + JSON 消息协议。
- CMake 3.21+。
- 腾讯地图 Web API / Qt WebEngineQuick（用户端地图导航）。
- Python 独立机器学习训练与预测服务。

禁止重新引入 Qt Widgets 作为用户端或管理端主界面。不得为了视觉效果引入新的大型 UI 框架、Web 前端框架、数据库或网络协议；确有必要时先说明原因、影响和迁移成本并询问用户。

## 5. 总体架构与职责边界

保持以下职责划分：

- QML：布局、视觉状态、轻量动画和用户交互。
- C++ ViewModel/Controller：页面状态、校验、筛选、异步请求和业务编排。
- `QAbstractListModel` / `QAbstractTableModel`：列表与表格数据。
- `libs/core`：数据库、Repository、TCP 服务、协议、密码安全和业务规则。
- Python `ml/`：训练、推理、数据导出和独立 JSON HTTP 服务。
- `libs/ui`：共享 QML 主题和基础视觉组件。

硬性要求：

- 页面 QML 不得直接执行 SQL。
- QML 不得包含数据库路径、管理员密码或服务密钥。
- GUI 线程不得执行阻塞式网络等待、耗时数据库查询或机器学习推理。
- 不要重写与 UI 任务无关的 Repository、数据库结构和 TCP/JSON 协议。
- 新增协议必须保持旧客户端兼容，并补充测试和文档。
- 缺失的数据能力不得伪装成真实接口。应在 ViewModel/Service 适配层隔离，并以 TODO、空状态或“演示数据”明确标识。
- 演示数据必须集中管理，禁止散落在多个页面组件中。

## 6. 重要目录

```text
apps/user-client/          QML 用户端
apps/admin-server/         QML 管理端，同时负责启动数据库和 TCP 服务
libs/core/                 核心业务、数据库、Repository、协议和网络
libs/ui/                   共享 QML 设计系统
database/                  SQLite schema 与种子数据
tests/                     Qt 自动测试
ml/                        Python 机器学习模块
docs/                      架构、协议、运行和验收文档
```

主要入口：

- 用户端 QML：`apps/user-client/qml/Main.qml`
- 用户端控制器：`apps/user-client/src/user_app_controller.*`
- 管理端 QML：`apps/admin-server/qml/Main.qml`
- 管理端控制器：`apps/admin-server/src/admin_app_controller.*`
- 共享主题：`libs/ui/qml/Theme.qml`
- 服务端路由：`libs/core/src/request_router.cpp`
- 数据库结构：`database/schema.sql`
- 预测服务：`ml/service.py`

## 7. 共享设计系统

管理端和用户端必须保持统一的品牌、颜色语义、字体、圆角、输入框、按钮、状态与动画语言。优先扩展和复用 `libs/ui`，不要为单个页面另建互相冲突的主题系统。

### 7.1 品牌与默认主题

- 品牌主色：青绿色，默认基准 `#0F9F8F`。
- 默认浅色背景：`#F4F7FA`。
- 默认表面：`#FFFFFF`。
- 默认主文字：`#172B3A`。
- 深色主题应保持沉稳、可靠和克制，避免高饱和赛博朋克效果。

### 7.2 语义令牌

页面必须从 `Theme.qml` 读取语义令牌，禁止在页面中通过主题名称逐项判断颜色。共享主题至少包括：

- `backgroundPrimary`、`backgroundSecondary`
- `surface`、`surfaceElevated`、`surfaceHover`、`surfaceSelected`
- `borderSubtle`、`borderStrong`
- `textPrimary`、`textSecondary`、`textMuted`
- `accent`、`accentHover`、`accentPressed`
- `success`、`warning`、`danger`、`info`
- `chartPalette`、`shadowColor`、`overlayColor`、`focusRing`
- 字号、间距、圆角、控件高度、行高和动画时长

当前主题至少包括：

1. 默认青绿 Charging Light
2. 深海青蓝 Midnight Cyan
3. 极光紫 Aurora Violet
4. 石墨橙 Graphite Orange
5. 翡翠绿 Emerald Grid
6. 云白蓝 Porcelain Light
7. 高对比 High Contrast

### 7.3 视觉与交互规范

- 使用 8 px 主间距体系，允许 4 px 半步。
- 页面边距通常为 20～28 px，桌面管理端当前采用 24 px。
- 卡片圆角 10～14 px，基础控件圆角约 8 px。
- 表格行高 42～48 px。
- 页面和抽屉动画 150～220 ms，使用轻微淡入/位移或 `OutCubic`。
- 不使用弹跳、持续闪烁、强呼吸灯、大面积实时模糊或高成本动态背景。
- 图标统一使用项目线性图标组件，不混合 Emoji、位图和不同粗细的图标。
- 状态不能只依赖颜色，必须同时显示文字或图标。
- 所有主题必须检查文字、边框、表格、弹窗和图表的可读性。
- 输入控件必须提供焦点状态；危险操作必须确认；异步操作必须提供加载与结果反馈。

## 8. 用户端约束

用户端定位为现代移动端风格，主要窗口约 440×820，最低适配约 390×680。

核心结构：

- 登录。
- 首页与充电站列表。
- 电站详情和电桩列表。
- 预约与充电流程。
- 个人中心、钱包和订单历史。
- 腾讯地图导航。
- 底部“首页 / 充电 / 我的”三标签导航。

业务要求：

- 手机号免密登录，首次登录自动注册。
- 开始充电后电桩状态必须同步为 `charging`。
- 充电计时基于真实订单开始时间，禁止使用固定时刻或时区错误造成从 08:00:00 开始。
- 停止充电、待结算、钱包扣款和订单历史必须由服务端状态驱动。
- 地图 Key 通过 `TENCENT_MAP_KEY` 提供，不写入代码或仓库。
- 不得因管理端改造破坏用户端已有页面、主题或业务流程。

## 9. 管理端约束

管理端定位为现代、专业、高密度但不拥挤的宽屏桌面运营系统：

- 主要设计基准：1440×900。
- 最低窗口：1280×720。
- 支持最大化和合理缩放。
- 结构为左侧导航、顶部状态栏和主内容区。

导航页面：

- 管理员登录。
- 数据总览。
- 电站管理。
- 电桩管理。
- 订单管理。
- 用户管理。
- 智能预测。
- 主题与设置。

管理端表格必须使用可复用代理和 C++ 模型，不通过大量静态 `Item` 或硬编码行数模拟表格。应支持搜索、筛选、滚动、空数据、加载、错误状态和详情抽屉。若后端尚无分页协议，要在文档中说明，不得伪装为已实现服务端分页。

危险操作规则：

- 删除电站、远程重启、冻结用户等必须二次确认。
- 确认文案中显示明确目标。
- 充电中的电桩不能重启。
- 删除存在关联订单的电站可能受外键保护，必须展示服务端返回原因。
- 操作期间禁止重复提交，并显示成功或失败反馈。

## 10. 数据库与协议兼容

- `database/schema.sql` 是数据库字段的事实来源。
- 修改数据库前先检查迁移和现有数据兼容性，不要直接假设字段存在。
- 当前数据库没有可靠的电桩心跳时间和电站更新时间字段。界面应显示“暂无心跳字段”或使用已有创建时间，不得生成虚假时间。
- 状态字符串必须沿用协议：
  - 电桩：`idle`、`charging`、`fault`、`offline`。
  - 用户：`active`、`frozen`。
  - 订单：`reserved`、`charging`、`awaiting_payment`、`completed`、`cancelled`。
- UI 层负责中文显示映射，不改变持久化状态值。
- 新增或修改管理接口时，至少覆盖鉴权、成功响应、非法参数和关键业务约束。

## 11. 机器学习模块

- `ml/` 由队友维护，修改前必须先拉取并检查远程最新提交。
- Qt GUI 不直接嵌入 Python 解释器或同步执行模型推理。
- 预测通过独立 HTTP 服务异步访问，默认地址：`http://127.0.0.1:8090`。
- 可通过环境变量 `CHARGING_ML_URL` 覆盖服务地址。
- 服务入口和接口定义以 `ml/service.py`、`ml/README.md` 为准，不凭空创建接口。
- 训练数据必须脱敏；预测服务不得直接修改订单、电桩或用户数据。
- 模型、数据或服务不可用时，UI 必须展示清晰的失败/重试状态；如使用演示数据，必须显示“演示数据”标识。
- 大型数据集、模型权重和训练产物不得提交到 Git，除非仓库已有明确的 LFS 方案且用户授权。

## 12. CMake 与 Qt 6.2 兼容

- 使用 `qt_add_executable` 和 `qt_add_qml_module` 打包 QML。
- 新增 QML 文件时必须加入对应 CMake `QML_FILES`。
- 共享模块 URI 为 `Charging.UI`。
- 共享 QML 静态模块需要保持正确的 `OUTPUT_DIRECTORY`、导入路径和资源初始化；修改后必须同时验证用户端与管理端启动。
- 不使用 Qt 6.3+、6.5+ 独有 API，除非提供 Qt 6.2 替代实现。
- 遇到“编译成功但类型不可用”时，检查 QML 模块 URI、qmldir、资源路径、静态插件注册和 `qrc:/` import path，不要简单复制组件规避共享设计系统。
- 不把构建产物写入源码目录。

推荐构建目录：

```text
/home/bit/charging-platform/build/admin-qml2
```

完整构建：

```bash
cd /home/bit/charging-platform
cmake -S . -B build/admin-qml2 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/admin-qml2 -j2
```

首次构建或共享 QML 变化后，QML 缓存生成可能耗时较长。不要因为短时间没有输出就重复启动并行构建；先检查现有 `cmake`/`gmake` 进程。

## 13. 自动测试与质量检查

基础回归测试：

```bash
cd /home/bit/charging-platform
ctest --test-dir build/admin-qml2 --output-on-failure
```

当前测试应至少保持：

- `core-tests`
- `repository-tests`
- `tcp-integration-tests`

修改业务协议、数据库或 Repository 时必须补充相应测试。UI 修改至少完成：

- CMake 全量构建。
- QML 启动冒烟测试。
- 相关页面和主题运行期日志检查。
- `git diff --check`。

无图形桌面时，可使用：

```bash
timeout 12s env \
  QT_QPA_PLATFORM=offscreen \
  QT_QUICK_BACKEND=software \
  ./build/admin-qml2/apps/admin-server/charging-admin
```

退出码 124 表示程序持续运行到超时，是冒烟测试的预期结果；必须同时确认日志中没有 QML 加载错误、绑定循环或未知属性。

测试环境如需自动登录，只能通过显式环境变量传入临时开发密码，例如 `CHARGING_ADMIN_SMOKE_PASSWORD`。不得把密码写入源码、测试日志、文档或 Git。

## 14. GUI 启动与 DISPLAY

VS Code Remote SSH 终端通常没有 Ubuntu 图形显示上下文。出现以下错误时：

```text
qt.qpa.xcb: could not connect to display
```

这通常不是 Qt 安装或程序代码故障，而是 `DISPLAY` 不可用。

人工验收优先在 VMware Ubuntu 图形桌面的终端运行：

```bash
cd /home/bit/charging-platform
./build/admin-qml2/apps/admin-server/charging-admin
```

管理端启动后，再打开第二个桌面终端运行用户端：

```bash
cd /home/bit/charging-platform
./build/admin-qml2/apps/user-client/charging-user
```

如果当前 Ubuntu 桌面确实使用 `:0`，且 SSH 会话有权限，可尝试：

```bash
export DISPLAY=:0
export XAUTHORITY=/home/bit/.Xauthority
```

不要在未检查 `/tmp/.X11-unix` 和当前图形会话的情况下猜测 DISPLAY。`QT_QPA_PLATFORM=offscreen` 只用于自动测试，不用于观察实际界面。

## 15. 安全与秘密信息

- 不在代码、文档、提交信息或命令输出中保存用户密码、sudo 密码、SSH 私钥、GitHub Token 或地图 Key。
- 用户在聊天中提供的密码只可用于当前明确授权的交互步骤，不得写入文件或长期记忆。
- sudo 应通过交互式终端输入，不要把密码拼接到命令、管道或脚本中。
- 不读取与任务无关的私钥、浏览器凭据或个人文件。
- 日志和截图中如出现手机号、Token 或敏感路径，应尽量脱敏。
- 用户管理页面默认可对手机号脱敏显示；业务协议仍保留必要的原始值。

## 16. 文件修改原则

- 优先小范围修改，保持模块边界清晰。
- 不创建超大 `Main.qml`；页面和主要组件拆分为独立文件。
- 避免复制粘贴重复颜色、尺寸和控件实现。
- 使用属性绑定和状态系统，避免到处命令式修改 UI。
- 避免循环绑定、无意义的深层 `anchors.fill` 和大量固定坐标。
- 不使用不存在的 QML 属性或高版本专属 API。
- 不删除用户端现有主题、页面或用户已有代码，除非当前重构明确替代它们并已完成验证。
- 删除被替代的代码前确认其不再参与构建，并确保 Git 历史可恢复。
- 不修改生成文件、构建目录内容或数据库运行文件。

## 17. 文档与已知限制

涉及以下内容时同步更新文档：

- 构建依赖或命令。
- 环境变量。
- 协议消息。
- 数据库字段。
- 新页面或关键交互。
- 机器学习服务启动方式。
- 已知限制和演示数据来源。

现有重要文档：

- `docs/admin-qml.md`
- `docs/user-client.md`
- `docs/protocol.md`
- `docs/architecture.md`
- `ml/README.md`

不要声称尚未实现的功能已经完成。最终汇报应明确区分：

- 已实现并验证。
- 已实现但需要人工环境验证。
- 使用演示数据或适配层。
- 尚未实现或受后端字段限制。

## 18. 最终交付格式

完成任务后，向用户简洁报告：

1. 实际完成的功能。
2. 关键文件或模块。
3. 构建、测试和运行验证结果。
4. 当前分支和提交号（如已提交）。
5. 是否已经推送及目标远程。
6. 运行命令。
7. 仍然存在的真实限制或需要人工确认的内容。

不要只提供设计建议或孤立代码片段；当用户要求实现时，应完成可运行修改并验证。不要因为任务较大而擅自降低为静态假页面，也不要为了满足截图效果破坏真实业务接口。
