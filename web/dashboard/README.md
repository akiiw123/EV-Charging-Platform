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
6. 站型运营效率
7. 工作日与周末对比
8. 起始电量健康度
9. 区域营收、成本和利润
10. 站点运营效率排行

其中时段、站型、周类型和区域均包含双指标或多指标对比。

## 数据来源

- 运营分析数据：Spark SQL → MySQL `charging_ads` → Flask，只读查询。
- 地理站点数据：OpenStreetMap 静态快照，© OpenStreetMap contributors，ODbL 1.0。
- 本目录不提交 SQLite 数据库、离线生成 HTML、截图或 `node_modules`。
