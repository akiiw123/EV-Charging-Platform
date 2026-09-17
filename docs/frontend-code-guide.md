# 前端代码注释与答辩导读

本文面向第一次阅读项目代码的同学，说明每类前端文件做什么、对接谁，以及关键函数在完整调用链中的作用。

## 1. 前端总体调用链

Qt 两端都遵循同一条链路：

```text
QML 页面 -> User/AdminAppController -> ApiClient -> TCP/JSON
         -> RequestRouter -> Repository -> SQLite -> 响应原路返回
```

- QML 只负责布局、状态展示和用户交互，不直接执行 SQL。
- Controller 是 ViewModel：校验页面输入、发异步请求、保存页面状态、更新列表模型。
- `libs/core` 才负责网络、业务规则和数据持久化。
- Web 大屏采用 `Vue -> Flask API -> MySQL ADS`，不直接修改交易数据。
- ML 预测由独立 HTTP 服务提供；界面只异步请求和展示结果。

## 2. 用户端 `apps/user-client`

### 启动与控制器

| 文件 | 作用及对接模块 |
|---|---|
| `src/main.cpp` | 用户端启动入口。注册 QML 类型，创建 `UserAppController`，注入 `appController`，加载 `qml/Main.qml`。 |
| `src/user_app_controller.h` | QML 可调用接口和可绑定属性的声明，是页面与 TCP 服务之间的正式边界。 |
| `src/user_app_controller.cpp` | 登录、定位、筛选、预约、充电、结算、资料、充值和地图的业务编排；通过 `ApiClient` 对接 C++ 服务端。 |
| `src/main_window.*` | 旧 Qt Widgets 客户端，保留用于历史兼容；当前正式用户端是 QML。 |

`UserAppController` 重点函数：

- `login(phone)`：校验手机号，发送 `user.login`；成功后继续加载用户资料、活动订单和站点。
- `refreshStations()`：请求电站列表，再加载各站电桩，最终按距离、价格、类型和空闲状态筛选。
- `locate(address)`：处理预设城市；配置 `TENCENT_MAP_KEY` 时异步调用真实地理编码。
- `selectStation(station)`：记录当前电站并加载它的电桩与计价规则。
- `loadPricing(stationId)`：获取固定价、分时价格和占位费规则，供详情及估算显示。
- `reserve(pileId, powerKw)`：创建预约，最终是否成功由服务端检查活动订单和电桩状态。
- `orderAction(action)`：把开始充电、停止、付款和取消统一分派成相应协议请求。
- `refreshProfile()`：刷新用户、历史订单和充值记录。
- `pickAvatar()`：选择图片、校验并裁剪为本地头像；不把任意原图路径直接交给页面。
- `openNavigation(mode)`：生成地图导航 URL，模式包括驾车、公交和步行。
- `sendRequest()` / `handleResponse()`：前者登记请求 ID，后者按 ID 和消息类型更新相应属性。
- `estimatedAmountFor()`：仅做充电中的界面估算；最终账单仍以服务端结算结果为准。

### 用户端页面

| 文件 | 具体职责 |
|---|---|
| `qml/Main.qml` | 用户端窗口和导航总入口；根据登录态、当前页和活动订单组织所有页面。 |
| `qml/Theme.qml` | 用户端主题适配层，将主题配置转换成页面易用的颜色、字号和间距。 |
| `pages/LoginPage.qml` | 手机号登录、输入校验、连接和错误状态。 |
| `pages/HomePage.qml` | 定位、搜索筛选、电站列表和进入详情。 |
| `pages/StationDetailPage.qml` | 电站、电桩、价格详情与预约入口。 |
| `pages/ChargingPage.qml` | 预约倒计时、充电时长和订单状态操作；计时基准来自服务端时间。 |
| `pages/OrdersPage.qml` | 活动订单和历史订单列表，负责状态中文化与详情展示。 |
| `pages/ProfilePage.qml` | 资料、头像、余额、充值记录和主题设置。 |
| `pages/MapPage.qml` | 承载腾讯地图网页，显示 Controller 准备好的地图或导航 URL。 |

### 用户端组件

| 文件 | 具体职责 |
|---|---|
| `components/AppButton.qml` | 统一按钮的主次、危险、禁用和焦点状态。 |
| `AppCard.qml` | 页面卡片的背景、边框、圆角和内边距。 |
| `AppDialog.qml` | 通用弹窗骨架。 |
| `AppField.qml` | 带标签、提示和焦点反馈的输入框。 |
| `AppIcon.qml` | 项目线性图标，避免混用 Emoji/位图。 |
| `AppScrollView.qml` | 长页面的统一滚动容器。 |
| `BottomNav.qml` | 底部功能导航。 |
| `EmptyState.qml` | 无数据、加载失败等统一提示。 |
| `StationCard.qml` | 首页电站摘要卡及详情点击事件。 |
| `StatusBadge.qml` | 协议状态到中文、图标和语义色的映射。 |
| `WheelArea.qml` | 桌面鼠标滚轮事件适配。 |

## 3. 管理端 `apps/admin-server`

### 启动与控制器

| 文件 | 作用及对接模块 |
|---|---|
| `src/main.cpp` | 初始化数据库与 TCP 服务，创建管理 Controller，再启动 QML 管理界面。 |
| `src/admin_app_controller.h/.cpp` | 管理页面的统一 ViewModel；对接 TCP 业务接口、列表模型、界面设置和 ML HTTP 服务。 |
| `src/json_list_model.h` | 把 JSON 数组包装成 `QAbstractListModel`，供 QML 表格按角色名读取。 |

`AdminAppController` 重点函数：

- `login()`：发送管理员登录请求；`remember` 只保存用户名，不保存密码。
- `refreshAll()`：登录成功后加载总览、电站、电桩、订单和用户。
- `refreshDashboard(days)`：获取指定日期范围的运营汇总和趋势。
- `refreshStations/Piles/Orders/Users()`：更新搜索和筛选条件，再刷新对应模型。
- `setStationRegionFilter()`：维护省、市、区三级筛选；空值表示不限制。
- `create/update/deleteStation()`：提交电站变更，删除限制由服务端和外键最终判断。
- `create/update/restartPile()`：管理电桩；充电中禁止重启的规则由服务端兜底。
- `setUserStatus()`：冻结或解冻用户。
- `loadPricing()/savePricing()`：读取和保存分时电价及占位费规则。
- `refreshPredictions()`：先取 ML 站点目录，再请求各站预测并聚合结果。
- `requestStationForecasts()`：控制并发的站点预测 HTTP 请求。
- `usePredictionDemo()`：真实预测不可用时进入明确标识的演示状态，不冒充真实模型。
- `request()/handleResponse()`：统一维护 TCP 请求、忙碌态、超时、模型更新和结果提示。

### 管理页面

| 文件 | 具体职责 |
|---|---|
| `qml/Main.qml` | 管理端窗口入口，切换登录页/主壳层并管理改密弹窗。 |
| `pages/LoginPage.qml` | 管理员账号密码登录与连接状态。 |
| `DashboardPage.qml` | KPI、趋势、电桩状态和站点能耗总览。 |
| `StationsPage.qml` | 电站搜索、三级地区筛选、编辑、删除和关联电桩跳转。 |
| `PilesPage.qml` | 电桩多条件筛选、创建编辑、状态修改和重启确认。 |
| `OrdersPage.qml` | 订单搜索、状态筛选和详情查看。 |
| `UsersPage.qml` | 用户查询、状态筛选和冻结/解冻。 |
| `PredictionPage.qml` | 1/6/24 小时预测、可信来源、模型名称、更新时间和错误状态。 |
| `SettingsPage.qml` | 主题、动画、字号、表格页大小和安全设置。 |

### 管理端组件

| 文件 | 具体职责 |
|---|---|
| `AppShell.qml` | 侧栏、顶部栏和页面栈的外壳，处理跨页面跳转。 |
| `Sidebar.qml` / `TopBar.qml` | 导航、连接状态、管理员信息及全局操作。 |
| `DataTable.qml` | 通用表格，负责表头、行代理、滚动、加载和空状态。 |
| `DetailDrawer.qml` | 在列表右侧展示完整详情。 |
| `ConfirmDialog.qml` | 危险操作二次确认并显示明确目标。 |
| `PricingDialog.qml` | 编辑固定/分时电价和占位费。 |
| `SearchField.qml` | 搜索输入、清空和提交。 |
| `FilterComboBox.qml` | 单选筛选。 |
| `MultiSelectComboBox.qml` | 多电站筛选，返回稳定 ID 集合。 |
| `MetricCard.qml` | 指标值、单位及趋势。 |
| `PageHeader.qml` | 页面标题、说明和页面级按钮。 |
| `ThemePreviewCard.qml` | 主题色板预览和选择。 |
| `EmptyState.qml` / `AppScrollView.qml` / `WheelArea.qml` | 空状态、滚动和桌面滚轮基础能力。 |

## 4. 共享设计系统 `libs/ui/qml`

| 文件 | 具体职责 |
|---|---|
| `Theme.qml` | 全项目语义颜色、字号、间距、圆角、控件高度和动画时长的事实来源。 |
| `AppButton.qml` | 共享按钮。 |
| `AppTextField.qml` | 共享输入框。 |
| `LineIcon.qml` | 共享线性图标。 |
| `PanelCard.qml` | 共享面板卡片。 |
| `StatusBadge.qml` | 共享状态徽标。 |

页面应读取 `Theme.accent`、`Theme.textPrimary` 等语义令牌，不能根据主题名称自行判断颜色。

## 5. Web 运营大屏 `web/dashboard/src`

| 文件 | 具体职责及关键函数 |
|---|---|
| `main.js` | 创建 Vue 应用、加载样式并挂载 `App.vue`。 |
| `App.vue` | 总页面；`refreshDashboard()` 定时拉数据，`loadStationData()` 加载地图，`syncTheme()` 处理自动日夜主题。 |
| `api/analytics.js` | 集中封装 Flask 请求；`fetchDashboard()` 取指标，`fetchStations()` 取站点，`fetchMlPrediction()` 请求预测。 |
| `components/DashboardCard.vue` | 图表卡片外壳及展开交互。 |
| `components/EChartPanel.vue` | 创建、更新和销毁 ECharts 实例，监听容器尺寸。 |
| `components/LiveOrders.vue` | 展示最新业务订单流，数据来自 Flask 实时只读接口。 |
| `components/MapPanel.vue` | 地图工具栏、2D/2.5D、缩放、省份切换和站点详情。 |
| `components/PredictionAssistant.vue` | 站点选择及 ML 预测卡片，明确显示真实/演示/失败状态。 |
| `lib/dashboard-model.js` | `normalizeDashboard()` 校验并标准化 JSON；`chartMissingReason()` 解释图表为何不能绘制。 |
| `lib/chart-options.js` | `buildChartOptions()` 将十组指标转换为 ECharts 饼图、雷达、柱线、散点和排行配置。 |
| `lib/geo-utils.js` | 地理坐标投影、边界和地图辅助计算。 |
| `lib/map-config.js` | 地图默认中心、缩放和视觉参数。 |
| `lib/map-view.js` | Canvas 地图主体；创建地图、更新站点、缩放、切换省份和释放事件资源。 |
| `lib/prediction-model.js` | 校验和整理预测响应，防止缺失/异常值直接进入图表。 |
| `styles.css` | 大屏布局、主题、响应式、焦点和减少动画规则。 |

Web 大屏接口链：

```text
App.vue -> api/analytics.js -> Flask /api/v1/*
        -> dashboard-model.js 校验 -> chart-options.js 生成配置
        -> EChartPanel/MapPanel 展示
```

## 6. 老师追问时的职责边界

- 页面展示错误时先看 QML/Vue；业务状态错误看 Controller、Router 和 Repository。
- Qt 页面不保存订单真相，SQLite 服务端状态才是事实来源。
- Web 大屏是只读分析系统，不能反向修改用户、订单和电桩。
- 预测结果来自独立 ML HTTP 服务；没有模型时必须显示失败或“演示数据”。
- 地图近似坐标只用于展示，不能称为真实 GPS。
