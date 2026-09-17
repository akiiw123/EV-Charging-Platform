# Web 大屏总览与 GLM 分支交接说明

这份文档帮助 `glm` 分支队友快速理解 Web 大屏目前已经完成的工作、数据从哪里来、每个文件负责什么，以及修改后如何验证。

## 1. 一句话理解大屏

Web 大屏是一个只读运营分析前端：Vue 3 负责页面状态和组件组织，DataV 负责大屏装饰，ECharts 负责图表，Canvas 负责全国站点地图，所有业务指标通过 Flask API 获取。

```text
SQLite 业务库/历史 CSV
        ↓
Spark 清洗与聚合
        ↓
MySQL ADS 结果表
        ↓
Flask /api/v1/*
        ↓
api/analytics.js
        ↓
dashboard-model.js 数据校验
        ↓
App.vue
   ├─ chart-options.js -> EChartPanel.vue
   ├─ MapPanel.vue -> map-view.js
   ├─ LiveOrders.vue
   └─ PredictionAssistant.vue -> ML 代理接口
```

大屏不直接写 SQLite 或 MySQL，不修改订单、电桩或用户状态。

## 2. 已完成的主要工作

- 从旧的原生 HTML/JavaScript 页面迁移到 Vue 3 + Vite。
- 接入 `@kjgl77/datav-vue3` 和 Apache ECharts。
- 对接 Flask 聚合接口，同时支持实时业务模式与历史批次模式。
- 展示 KPI、用户等级、用户雷达、终端平台、24 小时趋势、桩型负载、工作日/周末、SOC、区域收益和站点 TOP10。
- 实现全国站点 Canvas 地图、省份下钻、缩放、2D/2.5D、站点详情和连线动画。
- 实现日间、夜间和自动主题以及浏览器全屏。
- 实现定时刷新、超时取消、旧数据保留、错误提示和来源校验。
- 接入实时订单流和独立 ML 预测服务。
- 对缺失字段、近似坐标和演示预测做明确标注，不把未知值伪装成真实数据。

## 3. 目录和文件职责

### 页面入口

| 文件 | 作用 |
|---|---|
| `index.html` | Vite HTML 入口，提供 Vue 挂载节点。 |
| `src/main.js` | 创建 Vue 应用、加载全局样式并挂载根组件。 |
| `src/App.vue` | 大屏总控制器：KPI、图表组、地图、主题、全屏、定时刷新和错误状态。 |
| `src/styles.css` | 全局布局、主题变量、响应式、焦点样式及减少动画设置。 |

### 网络和数据校验

| 文件 | 作用 |
|---|---|
| `src/api/analytics.js` | 集中封装所有 `fetch`；组件不应自行散落接口地址。 |
| `src/lib/dashboard-model.js` | 校验 Flask 响应并把字符串数字转成有限数字；缺失值保留 `null`。 |
| `src/lib/prediction-model.js` | 从当前大屏数据生成规则预测所需的标准模型。 |

### 图表与卡片

| 文件 | 作用 |
|---|---|
| `src/components/DashboardCard.vue` | 统一卡片标题、角标、展开和键盘交互。 |
| `src/components/EChartPanel.vue` | 管理 ECharts 实例的创建、更新、ResizeObserver 和销毁。 |
| `src/lib/chart-options.js` | 把标准数据转换成各种 ECharts option。 |
| `src/components/LiveOrders.vue` | 轮询展示最新订单，只读取实时接口。 |

### 地图与预测

| 文件 | 作用 |
|---|---|
| `src/components/MapPanel.vue` | 地图 UI 外壳，负责工具栏、详情弹窗和用户操作。 |
| `src/lib/map-view.js` | Canvas 地图绘制与状态机：范围、相机、连线、动画和命中测试。 |
| `src/lib/geo-utils.js` | 多边形、投影、点归属省份、球面距离和邻近边计算。 |
| `src/lib/map-config.js` | 地图初始视角及配置常量。 |
| `src/components/PredictionAssistant.vue` | 打开预测面板、选择站点、请求并解释 ML 预测。 |

## 4. `App.vue` 的关键状态和函数

- `chartDataMode`：`live` 表示实时业务数据，`batch` 表示 Spark 历史批次。
- `dashboard`：标准化后的所有图表数据。
- `metadata`：批次、生成时间、脚本哈希和数据质量信息。
- `stationData`：地图使用的站点数据。
- `refreshDashboard()`：取消旧请求，设置 10 秒超时，获取指标并安排下一次刷新。实时模式 5 秒，批次模式 60 秒。
- `loadStationData()`：按当前模式加载真实业务站点或历史批次站点。
- `resolveAutoTheme()`：按本地时间 07:00～18:00 选择日间主题，其余时间使用夜间主题。
- `syncTheme()`：把主题写入根元素 `data-theme`，使 CSS 和图表同时更新。
- `toggleFullscreen()`：调用浏览器 Fullscreen API。
- `onBeforeUnmount()`：取消请求、计时器和事件监听，避免组件卸载后继续更新。

接口失败时不会清空上一份成功数据，而是在页面显示错误。这可以避免临时网络波动让大屏全部空白。

## 5. API 对接

### 批次分析

- `GET /api/v1/dashboard`：十组 ADS 指标和 metadata。
- `GET /api/v1/stations/map`：历史批次站点展示坐标。

### 实时业务

- `GET /api/v1/live/dashboard`：实时 KPI、订单、桩状态等。
- `GET /api/v1/live/stations`：业务站点及各状态电桩数量。
- 实时接口读取业务库，但仍然是只读的。

### ML 预测

- `GET /api/v1/ml/stations`：可预测站点目录。
- `POST /api/v1/ml/predict`：请求站点未来 1、6、24 小时预测。

所有请求经过 Flask 同源代理，因此前端不直接保存 MySQL 密码、ML 地址或其他秘密信息。

## 6. 数据校验为什么重要

`dashboard-model.js` 是后端 JSON 和图表之间的防线：

- `finite()`：只接受可转换为有限数字的值。
- `rows()`：按指标组检查数组和数字字段。
- `normalizeDashboard()`：验证响应 `code`、`data`、overview 和十组数组。
- `normalizeBusinessStations()`：统一实时/历史站点结构。
- `hasVerifiedSource()`：检查批次 ID、脚本路径和 SHA-256。
- `chartMissingReason()`：给出无法画图的真实原因，而不是填 0。

修改接口字段时必须同步修改这里和 `chart-options.js`，否则页面会主动报“响应格式不正确”。

## 7. 图表口径

- `sessions`：充电会话次数。
- `total_kwh`：累计充电电量。
- `total_fee`：历史批次按对应 ADS 口径；实时模式显示已结算营收。
- `abnormal_rate`：清洗、去重或关联校验剔除比例，不是故障率。
- `utilization_rate`：同组内相对负载，不是设备时间利用率。
- `battery_health`：当前实际展示起始 SOC 分布，不应说成电池健康诊断。
- 区域成本：`kWh × 批次 cost_per_kwh` 的估算结果。
- 历史站点坐标：原始数据无经纬度时按站名生成展示锚点，不是真实 GPS。

## 8. 地图实现

地图没有使用 ECharts 地图层，而是由 `map-view.js` 在 Canvas 上绘制：

1. `prepareRegions()` 整理中国 GeoJSON。
2. `createProjection()` 根据画布、2D/2.5D 和相机状态生成投影函数。
3. `locateProvince()` 判断站点属于哪个省。
4. `proximityEdges()` 根据球面距离生成有限数量的邻近连线。
5. 动画循环绘制地图、站点光点和能量流动效果。
6. 点击时做命中测试，将站点交给 `MapPanel.vue` 打开详情。

系统设置“减少动态效果”时应关闭非必要动画；窄屏会自动改成普通文档布局。

## 9. 本地运行

先启动 Flask 分析服务：

```bash
cd /home/bit/charging-platform/analytics
set -a && . ./.env && set +a
.venv/bin/gunicorn -c deploy/gunicorn.conf.py wsgi:app
```

开发模式：

```bash
cd /home/bit/charging-platform/web/dashboard
npm ci
npm run dev
```

Vite 默认访问 `http://127.0.0.1:5173`，并把 `/api/v1` 代理到 `ANALYTICS_PROXY_TARGET`，默认是 `http://127.0.0.1:8091`。

生产构建：

```bash
npm run test
npm run build
```

构建产物位于 `dist/`，Flask 会挂载到：

```text
http://<虚拟机IP>:8091/dashboard/
```

## 10. 修改后的检查清单

1. `npm run test`：验证数据转换逻辑。
2. `npm run build`：验证 Vue 模板和依赖。
3. `python tests/browser_smoke.py`：浏览器冒烟测试。
4. 同时检查实时/批次两个模式。
5. 检查日间、夜间、自动主题和全屏。
6. 断开 Flask 或 ML 服务，确认错误状态真实且不会显示伪数据。
7. 检查 1280×720 和窄屏布局。
8. `git diff --check`，确保没有尾随空格。

## 11. 常见修改入口

| 需求 | 优先修改 |
|---|---|
| 新增后端指标 | Flask 路由 → `dashboard-model.js` → `chart-options.js` → `App.vue` |
| 修改图表样式 | `chart-options.js`，不要在多个组件重复配置 |
| 修改页面布局 | `App.vue` + `styles.css` |
| 修改地图效果 | `map-view.js`；几何算法放 `geo-utils.js` |
| 修改接口地址/模式 | `api/analytics.js` + `vite.config.js` |
| 修改预测展示 | `PredictionAssistant.vue`，协议变化同时检查 Flask ML 路由 |
| 修改主题颜色 | `styles.css` 的主题变量及 `chart-options.js` 的图表调色板 |

## 12. 已知限制

- Web 大屏定位为局域网只读分析工具，尚未加入正式部署所需的登录、TLS 和细粒度授权。
- Spark/MySQL 是批处理链路，不是毫秒级流处理。
- 历史数据缺少的字段不会由前端猜测补齐。
- ML 模型、数据或服务不可用时只能显示错误或明确的演示状态。
- OSM/GeoJSON 静态数据和展示锚点不能当作实时设备定位。
