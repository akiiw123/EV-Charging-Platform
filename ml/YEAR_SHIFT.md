# 2024–2025 年历史时间调整与复测

日期：2026-09-15。按用户指定规则，ML基于用户本地版本调整。下述年份调整复测以main `3e838ef`为当时基线；发布分支随后基于更新的main创建，并再次执行ML测试。

## 调整范围

原始数据目录 `Documents/04.数据集最终版` 的订单 `created/ended` 从 `0014/2014`、`0015/2015` 分别调整到2024、2025。
3395条订单、6790个起止时间更新；weekday及七个星期哑变量重新计算；电量、时长和其他非日历字段不变。
站点2019年更新时间、电池原始 record_time 未改。备份和前后SHA256在本次交付报告目录中。

原始数据范围映射为2024-11-18至2025-10-04。训练2025年3–7月、验证8月、测试9月；默认共同回放时刻2025-09-03 00:00。
这些是调整日期后的历史记录，并非2024、2025或2026年实采数据。模型不能据此声称具备2026年实时预测能力。

## 实现

- `session_dates.py`：显式、可重复执行的年份映射，拒绝非法/未指定年份；支持已有脱敏数据。
- `train_sessions.py`：同步时间划分，原始CSV重算日期特征，模型版本包含shifted标识；可用 `--cleaned-data` 从脱敏产物重训。
- `session_engine.py`、`service.py`：2025年9月回放及错误提示；响应附 `date_shift` 来源信息。
- `artifacts/sessions`：从年份调整后的原始三份CSV重新训练生成，未复用旧模型权重冒充新结果。

## 运行

在项目根目录、安装对应依赖后：

```bash
python ml/train_sessions.py --data-dir '/path/to/04.数据集最终版' --artifacts ml/artifacts/sessions
python ml/service.py --backend sessions --artifacts ml/artifacts/sessions
python -m unittest discover -s ml -p 'test_*.py' -v
python ml/smoke_sessions.py --artifacts ml/artifacts/sessions
```

服务默认仅监听127.0.0.1:8090。使用 `/stations` 返回的站点ID。2026年的回放请求仍被拒绝，因为不存在对应实测历史。

## 复测与限制

ML 22项单元/兼容测试通过；HTTP基础冒烟通过；76站点六并发的1824个曲线点及9项常规错误请求通过。
独立重算12组selected指标与保存报告一致。选型仍为seasonal，整体电量MAE约0.1041kWh；零预测基准约0.0809，不能宣称高精度运营收益。

已知未修复：超过1MiB请求体在本机可能得到连接重置而非413；扩展报告真实标为失败。
远程管理端仍忽略历史回放来源、将缺失区间显示成0%、将空闲桩期望值截为整数；本次遵守非ML以远程为准，没有修改管理端。
Qt桌面构建因本机缺Qt6开发包未完成；不将HTTP/源码检查称作GUI运行验收。
