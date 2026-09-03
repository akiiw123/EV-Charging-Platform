# 功能完善计划(Improvement Plan)

> 分支:`glm` · 建立日期:2026-09-03 · 依据:对 glm 分支代码的逐子系统核查
> 说明:本文取代 `project-status-and-team-plan.md` 中已过时的完成度结论(该文档评估基线是旧
> main 提交,其中"ml 仅预留目录""活动订单无引导"等结论已不再成立)。

## 1. 现状盘点(未完善功能清单)

### A. 智能预测:前后端断接 ⭐ 优先级最高

- `ml/` 目录已有完整实现:`train.py`(训练)、`export.py`(自有数据导出)、`service.py`(推理服务,
  提供 `GET /health`、`GET /stations`、`POST /predict`,输出 1/6/24h 负荷点估计 + 90% 置信区间
  + 24h 曲线 + 预计空闲桩)。
- 但管理端 `AdminAppController::refreshPredictions()` 只调用了 `/stations` 拿站点列表,
  表格行填"等待推理"占位;`/predict` **从未被调用**。
- 预测页 4 个指标卡显示静态文字("短时负荷"等),可信度列在服务在线时写死 90。
- 服务不可达时展示硬编码演示数据(此降级策略保留)。

### B. 用户端(apps/user-client)

| # | 问题 | 位置 | 优先级 |
|---|------|------|--------|
| B1 | 定位是硬编码的三城市坐标,无真实 GPS/地址解析;距离计算与排序为真 | `user_app_controller.cpp locate()` | 中 |
| B2 | 头像未实现:服务端支持 `avatar_path`,QML 无任何引用,仅显示昵称首字母 | `ProfilePage.qml` | 中 |
| B3 | 地图为腾讯 URI 外链,Key 缺失时部分环境不可用;无站内地图 | `MapPage.qml` | 低 |
| B4 | 并发请求无 ID 映射,旧响应可能覆盖新状态(`send()` 已返回 uuid 但未用) | `api_client.cpp` / `handleResponse` | 中 |
| B5 | `src/views/` 下 6 对 Widgets 旧视图为死代码,仍在编译 | `apps/user-client/src/views/` | 低 |

### C. 管理端(apps/admin-server)

| # | 问题 | 优先级 |
|---|------|--------|
| C1 | `must_change_password` 服务端已返回,前端无强制改密页;默认口令 admin/123456 长期可用 | 高 |
| C2 | 电桩管理仅支持远程重启,不能手工设置故障/离线,不能单独增删电桩 | 中 |
| C3 | 电桩无心跳字段(页面已自我标注"暂无心跳字段") | 低 |
| C4 | 冻结用户不踢已建立会话,仅拦截下次登录 | 中 |

### D. 服务端与协议(libs/core)

| # | 问题 | 优先级 |
|---|------|--------|
| D1 | 无 TLS/会话令牌/心跳/空闲连接超时/速率限制 | 低(课程演示) |
| D2 | 金额使用 double,存在浮点误差 | 低 |
| D3 | `request_router.cpp` 约 571 行,全部业务集中单文件 | 中(可维护性) |
| D4 | 预约超时 15 分钟为固定值,不可配置 | 低 |

### E. Web 大屏(web/dashboard)

| # | 问题 | 优先级 |
|---|------|--------|
| E1 | ECharts 走 jsdelivr CDN,离线环境整页空白 | 高(一行改动) |
| E2 | 直接读 SQLite,绕过统计接口约定 | 低 |
| E3 | 指标维度少:无站点排行/时段分布/利用率;趋势固定 7 日 | 中 |

### F. 测试与文档

- 无测试覆盖:`admin.station.update/delete`、`admin.dashboard` 的 days 参数、头像更新。
- `project-status-and-team-plan.md` 已失真,需重写(本文档部分替代)。

## 2. 实施顺序

| 步骤 | 内容 | 对应缺口 | 状态 |
|------|------|----------|------|
| 1 | **管理端真正接入 ml `/predict`**:逐站点请求预测,表格与指标卡显示真实数值 | A | ✅ 本次完成 |
| 2 | 强制改密页 + 首登改密流程 | C1 | ✅ 本次完成 |
| 3 | 头像选择、预览与展示 | B2 | ✅ 本次完成 |
| 4 | Web 大屏 ECharts 本地化 | E1 | 待做 |
| 5 | 状态文档重写 + 新接口测试补充 | F | 待做 |
| 6 | 电桩手工状态管理 / 并发请求 ID 映射 / 请求路由拆分 | C2/B4/D3 | 备选 |

## 3. 步骤 1 详细设计:管理端接入 /predict

### 3.1 接口契约(ml/service.py)

- `GET /stations` → `{"count":N,"stations":[{"station_id":"559","total_piles":8},...]}`
- `POST /predict` 请求体 `{"station_id":"559"}`(其余字段缺省回退数据集)→ 响应:
  ```json
  {
    "total_piles": 8,
    "quantiles": [0.05, 0.5, 0.95],
    "load_kwh":         {"1": {"point":42.6,"lower":..,"upper":..}, "6": {...}, "24": {...}},
    "available_piles":  {"1": {"point":3,"lower":..,"upper":..}, "6": {...}, "24": {...}},
    "curve": [{"offset":1,"timestamp":"...","load_kwh":..,"busy_ratio":..}, ... 共 24 项]
  }
  ```
- 服务地址:`CHARGING_ML_URL` 环境变量,默认 `http://127.0.0.1:8090`。

### 3.2 数据流

```
refreshPredictions()
  └─ GET /stations(超时 4s)
       ├─ 失败 → usePredictionDemo(原因)          【保留现有降级】
       └─ 成功 → 取前 6 个站点,并发 POST /predict
            ├─ 单站成功 → 行数据:
            │     h1/h6/h24 = load_kwh["1"/"6"/"24"].point,格式 "42.6 kWh"
            │     free       = available_piles["1"].point
            │     risk       = available_piles["1"].point==0 或峰值 busy_ratio≥0.9
            │                  → "容量预警";否则取曲线 busy_ratio 峰值小时 → "18:00 高峰"/"正常"
            ├─ 单站失败 → 该行 h1/h6/h24 填 "—",状态栏注明
            └─ 全部完成 → 聚合求和写入指标卡属性,emit predictionChanged
```

### 3.3 新增 C++ 属性(AdminAppController)

| 属性 | 类型 | 含义 |
|------|------|------|
| `predictionLoad1` / `predictionLoad6` / `predictionLoad24` | QString | 全部站点 1/6/24h 预测负荷合计,如 "512.4 kWh";演示模式为 "—" |
| `predictionConfidence` | QString | 由响应 quantiles 计算,如 "90";演示模式为 "—" |

### 3.4 页面改动(PredictionPage.qml)

- 指标卡"未来 1/6/24 小时"的 `value` 由静态文字改为绑定上述三个属性;
- "可信度"卡改为绑定 `predictionConfidence`。

### 3.5 验收标准

1. 预测服务不在线:行为与现在一致(演示数据 + 明确标注"演示数据"),不崩溃。
2. 服务在线且模型产物齐全:表格 1/6/24h 列显示数值(非"等待推理"),指标卡显示合计负荷,
   风险列出现"高峰/容量预警"判定。
3. 单站点预测失败不影响其余站点行;整体请求有超时,UI 不阻塞。
4. `cmake --build` 零错误;现有 ctest 全绿(预测链路无自动化测试,靠 1/2 手工验证)。

## 4. 变更记录

- 2026-09-03 建立本文档;步骤 1 实施(glm 分支)。
- 2026-09-03 步骤 1 完成:`refreshPredictions()` 改为"GET /stations → 逐站点并发 POST /predict",
  新增 `predictionLoad1/6/24`、`predictionConfidence` 属性;预测页指标卡绑定真实聚合值;
  单站失败行内标"—",服务整体不可达仍降级演示数据。
  联调辅助:虚拟机 `/tmp/mock_ml.py` 为无 torch 依赖的契约模拟服务(`python3 /tmp/mock_ml.py`,
  默认 127.0.0.1:8090),可先用于界面联调;正式演示请运行 `ml/service.py` + `train.py` 产物。
- 2026-09-03 步骤 2 完成:新增 `admin.password.change` 接口(校验旧密码 → 强度检查 →
  PBKDF2 落库并清除首登标志,仓储层 `AdministratorRepository::changePassword`),
  管理端登录后若 `must_change_password` 为真则弹出不可关闭的强制改密弹窗,
  成功后自动放行;新增集成测试 `forcedPasswordChangeFlow` 覆盖全链路。
- 2026-09-03 步骤 3 完成:用户端头像功能落地——点击头像弹出系统文件选择器
  (QFileDialog,用户端为此链接 QtWidgets),校验格式与 5MB 大小后居中裁方缩放
  256×256 并绘制为圆形透明 PNG 存入应用数据目录(按手机号命名,避免原图片被
  移动后失效),路径经 `user.profile.update` 持久化;界面图片优先、昵称首字母
  兜底,保存后即时预览。新增集成测试 `profileAvatarPathUpdate`。
  附带修复:管理端 Main.qml 补 `import QtQuick.Layouts`(改密弹窗用到);
  用户端 main.cpp 显式 `QQuickStyle::setStyle("Basic")`,避免链接 Widgets 后
  默认切到 Fusion 样式。
  环境说明:本虚拟机(192.168.202.128)缺少 QtQuick 运行时 QML 模块,GUI 启动
  需补装 `sudo apt install qml6-module-qtquick-templates qml6-module-qtquick-window
  qml6-module-qtwebengine`;已用免 root 方案验证:`~/qt-extra-qml/` 下解包了
  上述 deb,运行前 `export QML_IMPORT_PATH=~/qt-extra-qml/usr/lib/x86_64-linux-gnu/qt6/qml`。
