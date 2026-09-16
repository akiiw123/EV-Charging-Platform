# Vue3 + DataV 运营分析大屏

大屏位于 `web/dashboard`，使用 Vue3、DataV 和 Apache ECharts，将 Spark SQL 产出的 10 组 ADS 结果通过 Flask 统一接口展示。页面不读取 Qt 的 SQLite 业务库，也不在接口失败时伪造运营数据。

## 数据链路

```text
CSV -> HDFS/ODS -> Spark SQL DWD/DWS/ADS -> MySQL charging_ads
    -> Flask /api/v1/dashboard -> Vue3/DataV/ECharts
```

Flask 只读查询分析结果；Spark 导出、MySQL 导入和接口配置见 `analytics/README.md`。

## 构建前端

要求 Node.js 23 或更高版本：

```bash
cd /home/bit/EV-Charging-Platform/web/dashboard
npm ci
npm run test
npm run build
```

`dist/` 是本地构建产物，不提交 Git。Vite 开发服务器可用 `npm run dev` 启动，默认将 `/api/v1` 代理到 `http://127.0.0.1:8091`；需要连接其他地址时设置 `ANALYTICS_PROXY_TARGET`。

## 启动 Flask/Gunicorn

```bash
cd /home/bit/EV-Charging-Platform/analytics
set -a
. ./.env
set +a
.venv/bin/gunicorn -c deploy/gunicorn.conf.py wsgi:app
```

浏览器访问：

```text
http://<bitdev-ip>:8091/dashboard/
```

Flask 默认托管 `web/dashboard/dist`，可用 `DASHBOARD_DIST_DIR` 覆盖。`/` 会重定向到 `/dashboard/`，接口继续位于 `/api/v1`。

## 展示内容

页面包含核心 KPI、用户等级、用户行为雷达、终端平台、24 小时趋势、站型相对负载与金额、工作日/周末对比、起始电量（SOC）分布、区域已结算营收、估算电量成本与估算利润、站点充电次数 TOP10，共 10 个分析维度；其中多个面板包含双指标或多指标对比。图表类型包括折线图、柱状图、环形图、雷达图、面积图和组合图。

全国地图复用 Canvas 交互实现，站点通过 `/api/v1/live/stations` 从 SQLite 业务库只读加载。地图站点数量、覆盖站点 KPI 和站点排行使用同一业务数据源；底图边界来自本地化的中国 GeoJSON。

## 验证

```bash
cd /home/bit/EV-Charging-Platform/web/dashboard
npm run test
npm run build
python3 tests/browser_smoke.py http://127.0.0.1:8091/dashboard/

cd /home/bit/EV-Charging-Platform/analytics
PYTHONPATH=. python -m unittest discover -s tests -v
ANALYTICS_BASE_URL=http://127.0.0.1:8091 PYTHONPATH=. python tests/smoke_http.py
```

浏览器冒烟脚本需要当前 Python 环境已安装 Playwright 与 Chromium；常规前端单元测试和生产构建不依赖它。

## 已知限制

- 课程环境默认使用局域网 HTTP；正式公网部署需要 HTTPS、鉴权、反向代理和监控。
- `dist/` 必须先构建，缺失时 `/dashboard/` 返回统一格式的 404。
- 静态地图站点与 MySQL 运营分析数据来源不同，页面明确区分两者。

## 指标口径（PR #23）

- 充电会话为 charging / awaiting_payment / completed 订单；已结算营收仅计 completed 的 amount + occupancy_fee。
- abnormal_rate 表示清洗、去重及关联校验剔除订单的比例，不再是 SOC 缺失率。
- 相对负载为充电次数 / 同组最大次数，非设备时间利用率；SOC 为起始电量分布，非电池健康诊断。
- 成本使用分析批次 quality.cost_per_kwh，不在页面写死；周末按上海时区的周六、周日划分。
- TOP10 沿用 rn 排序并显示次数；负载与营收关系图使用同组 TOP10 的 utilization_rate 与 total_fee，缺失值不绘制为零。
- 页面“指标口径与数据来源”显示批次及生成时间；缺少元数据时明确提示待核验。需要完成新版分析导入，旧服务返回数据不等于新链路验证通过。
