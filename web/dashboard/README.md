# VoltFlow Vue3 + DataV 分析大屏

本目录是第二阶段大数据分析前端。页面通过统一 Flask API 读取 Spark SQL 导入 MySQL 的 10 组 ADS 结果，并保留 V3 全国 OSM 充电站地图能力。

## 技术栈

- Node.js 23+
- Vue 3 + Vite
- `@kjgl77/datav-vue3`
- Apache ECharts
- Flask `/api/v1/dashboard`

页面不使用虚构运营指标。接口失败时保留上次成功结果并显示错误；静态 OSM 站点不被解释为实时电桩状态。

## 开发运行

先启动 Flask/Gunicorn `8091` 服务，再运行：

```bash
cd web/dashboard
npm ci
npm run dev
```

Vite 默认监听 `5173`，并把 `/api/v1` 代理到 `ANALYTICS_PROXY_TARGET`，默认值为 `http://127.0.0.1:8091`。

## 构建与部署

```bash
npm run test
npm run build
```

构建结果写入忽略目录 `dist/`。Flask 会在构建目录存在时提供：

```text
http://<bitdev-ip>:8091/dashboard/
```

## 分析维度

1. 核心业务 KPI
2. 用户等级分布
3. 用户行为雷达
4. 终端平台偏好
5. 24 小时充电趋势
6. 站型相对负载与金额
7. 工作日与周末对比
8. 起始电量（SOC）分布
9. 区域已结算营收、估算电量成本和估算利润
10. 站点充电次数 TOP10

其中时段、站型、周类型和区域均包含双指标或多指标对比。

## 数据来源

- 运营分析数据：Spark SQL → MySQL `charging_ads` → Flask，只读查询。
- 地理站点数据：OpenStreetMap 静态快照，© OpenStreetMap contributors，ODbL 1.0。
- 本目录不提交 SQLite 数据库、离线生成 HTML、截图或 `node_modules`。

## 指标口径（PR #23）

- 充电会话为 charging / awaiting_payment / completed 订单；已结算营收仅计 completed 的 amount + occupancy_fee。
- abnormal_rate 表示清洗、去重及关联校验剔除订单的比例，不再是 SOC 缺失率。
- 相对负载为充电次数 / 同组最大次数，非设备时间利用率；SOC 为起始电量分布，非电池健康诊断。
- 成本使用分析批次 quality.cost_per_kwh，不在页面写死；周末按上海时区的周六、周日划分。
- TOP10 沿用 rn 排序并显示次数；负载与营收关系图使用同组 TOP10 的 utilization_rate 与 total_fee，缺失值不绘制为零。
- 页面“指标口径与数据来源”显示批次及生成时间；缺少元数据时明确提示待核验。需要完成新版分析导入，旧服务返回数据不等于新链路验证通过。

## 大屏显示与交互

- 桌面采用中央沉浸地图、两侧浮动分析卡片；“用户与时段 / 结构与收益”切换侧栏图表，全部十组分析维度保留，站点排行常驻。
- 夜间卡片使用较亮的蓝灰表面、可辨识边界，白天站点使用青绿色光晕。灯光只表示 OSM 位置，不代表实时充电状态。
- 灯光尊重系统减少动态效果设置；窄屏自动改为正常文档布局。侧栏、KPI 与浮动卡片支持键盘聚焦展开。
