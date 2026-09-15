# 智能分析模块

该目录提供独立 Python 训练与预测服务，默认保留现有 LSTM 后端。新增 `sessions` 后端适配项目提供的 `nvv2t.csv`、`nvv2t_md_end.csv` 订单数据，供管理端已有智能预测页面进行历史回放。

## 年份调整（2026-09-15）

按照用户要求，原始订单的 `0014/2014` 映射为 `2024`，`0015/2015` 映射为 `2025`。
这属于历史时间调整，不是2024、2025或2026年的真实采集数据。星期特征会重新计算，模型必须重新训练。
`date_shift` 响应字段和模型版本标识保留迁移来源；仍为 `historical_replay`。
原始目录中的订单 CSV 同步调整；2019年站点更新时间及电池原始记录时间不在本次映射范围。
详细复测和限制见 [年份调整说明](YEAR_SHIFT.md)。

## 订单数据后端（本次适配）

与 Qt 已有接口一致：默认 `http://127.0.0.1:8090`，提供 `GET /health`、`GET /stations`、`POST /predict`。业务 TCP 协议、数据库结构及用户端保持不变。

在仓库根目录执行：

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r ml/requirements-sessions.txt
python ml/train_sessions.py --data-dir '/实际路径/04.数据集最终版' --artifacts ml/artifacts/sessions
python ml/service.py --backend sessions --artifacts ml/artifacts/sessions
```

如果使用随交付提供的已训练模型包，将其解压至仓库根目录，得到 `ml/artifacts/sessions/model.joblib`，可跳过训练。模型和训练数据不加入 Git。服务端只加载可信模型文件。

Qt 管理端默认地址无需修改；如服务运行在另一台开发机，通过原环境变量设置 `CHARGING_ML_URL`。打开管理端的“智能预测”页即可请求服务。

```bash
curl http://127.0.0.1:8090/stations
curl -X POST http://127.0.0.1:8090/predict \
  -H 'Content-Type: application/json' \
  --data '{"station_id":"129465","horizons":[1,6,24]}'
```

实际数据重新训练后，可直接使用 `ml/artifacts/sessions/example_request.txt` 中的已生成请求。默认回放起点在 2025 年 9 月中按可用站点最多选取，所有站点共用该起点；`/stations.default_timestamp` 返回具体时间。可用 `--replay-at '2025-09-03T00:00:00'` 显式指定回放起点。列表仅包含该起点前至少 7 天历史、后面完整 24 小时在观察区间内的站点。

关键返回约定：

- `load_kwh["6"].point` 是第 6 个小时区间的电量，不是前 6 小时累计电量。`curve` 保留完整 24 点，并补充 `interval_start/interval_end`。
- `available_piles` 为小时开始时空闲数量的期望值，可以是小数；不能代替预约系统的实时可用桩数。
- `mode=historical_replay`、`station_namespace=session_dataset` 明确表示历史数据集。数据集 `stationId` 与平台 `charging_stations.id` 没有已验证映射，不能按编号或排序位置直接绑定。
- 点预测暂未校准预测区间，返回 `quantiles=[]`、`lower=null`、`upper=null`；管理端显示“未提供预测区间”。
- `selected_models` 说明实际采用的是 ML 或季节均值；不会把基准算法伪装成深度学习预测。
- `sessions` 后端拒绝当前年份、未知站点、非法步长，以及不支持的天气/实时历史覆盖字段，返回 HTTP 400。

训练过程保留时间划分：3～7 月训练，8 月选择模型，9 月测试；完整小时标签必须在分区截止前可用。输入只使用截至预测起点已知的事件和已完成订单电量。训练产物不包含用户、手机号、订单 ID 或终端平台字段。

当前结果仍受数据稀疏限制：验证集选择季节均值，零预测的总体 MAE 更低；不能据此宣称高精度运营预测。详细口径、限制、实际回测和虚拟机验收见 [适配说明](YEAR_SHIFT.md) 及训练生成的 `artifacts/sessions/REPORT.md`。

只有脱敏后的现有产物、没有原始 CSV 时也可重训（明确记录输入来源）：

```bash
python ml/train_sessions.py --cleaned-data /path/to/old-session-artifacts --artifacts /path/to/new-session-artifacts
```

自动验证：

```bash
# sessions 单元与协议适配测试；若没有 torch，LSTM 测试明确跳过
python -m unittest discover -s ml -p 'test_*.py' -v
# 原 LSTM 兼容性测试需要先安装其原有依赖
python -m pip install -r ml/requirements.txt
python -m unittest discover -s ml -p 'test_*.py' -v
# 实际模型 HTTP 检查，临时监听本机随机端口，结束后关闭
python ml/smoke_sessions.py --artifacts ml/artifacts/sessions
```

## 原 LSTM 后端

原 `data.py`、`model.py`、`train.py`、`export.py` 保持原有入口；不传 `--backend` 时仍启动 LSTM，不要求安装 scikit-learn。示例：

```bash
python ml/train.py --data-dir ml/data/own --artifacts ml/artifacts/lstm
python ml/service.py --backend lstm --data-dir ml/data/own --artifacts ml/artifacts/lstm
```

从数据集读取历史时返回 `mode=historical_replay`，显式传入历史时返回 `mode=provided_history`。本次还修正了历史窗口不足及时间越界时的静默错位，改为明确报错。

以下为原有数据布局说明。

建议输入：站点 ID、时间特征、历史负荷、天气、节假日；建议输出：未来 1/6/24 小时负荷、空闲桩数量和置信区间。

训练数据必须脱敏，模型不可直接修改订单或电桩状态。

## 数据来源与自有数据兼容

模型输入统一为“站点 x 小时”标准 CSV 布局（与 [UrbanEV](https://github.com/IntelligentSystemsLab/UrbanEV) zone 级数据一致），由 `data.load_urbanev` 加载；任何符合该布局的数据目录都能直接用于 `train.py` / `service.py`。

自有平台数据用 `export.py` 从 SQLite 导出为同一格式，训练与推理代码零改动：

```
python export.py --db ../database/your.db --out ./data/own
python train.py --data-dir ./data/own --artifacts ./artifacts
python service.py --data-dir ./data/own --artifacts ./artifacts
```

- 站点 ID 即 `charging_stations.id`；负荷为订单 `energy_kwh` 按时间重叠分摊到小时；占用为每小时活跃订单的桩·小时 / 站点总桩数；电价取 `price_per_kwh`，服务费暂填 0；天气常数填充（可用 `--weather-csv` 换成实测）。

- 导出仅含站点级聚合、无用户字段，且以只读方式打开数据库，满足脱敏与“不修改订单/电桩状态”约束。

- 建议积累 4 周以上数据再训练自有模型；期间可继续使用 UrbanEV 模型提供服务。

## GitHub 分支交付范围

本分支只包含ML源码、自动测试及说明，不包含原始数据、模型权重或训练产物。首次运行须按上述命令使用本地数据完成训练。
