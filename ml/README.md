# 智能分析模块

该目录预留负荷预测服务。基础业务完成后，建议采用独立 Python 服务并提供稳定的 JSON API，避免将 Python 运行时直接耦合进 Qt GUI。

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

