# 智能分析与负荷预测模块

`ml/` 是与 Qt GUI 解耦的 Python 机器学习模块，包含脱敏数据导出、特征构造、PyTorch 训练和只读 HTTP 推理。预测服务不修改订单、电桩或用户数据。

## 文件结构

- `export.py`：从平台 SQLite 只读导出按“站点 × 小时”聚合的数据。
- `data.py`：加载 UrbanEV 或同结构自有数据，构造负荷、占用率、时间、天气和价格特征。
- `model.py`：负荷预测网络及分位数损失。
- `train.py`：训练/验证拆分、早停、评估并输出 `model.pt` 和 `meta.json`。
- `service.py`：加载产物并提供 JSON HTTP API。
- `requirements.txt`：Python 依赖范围。

## 安装依赖

建议在项目外的虚拟环境安装，模型和数据产物不要提交 Git：

```bash
cd /home/bit/charging-platform/ml
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## 数据格式与自有数据

输入采用 UrbanEV 风格的“站点 × 小时”CSV。自有平台数据可直接从 SQLite 脱敏导出：

```bash
python export.py --db ../charging_platform.db --out ./data/own
```

导出结果只保留站点级聚合，不含用户 ID、手机号或订单身份信息。建议积累至少四周数据再训练；数据不足时可使用兼容的公开 UrbanEV 数据完成流程演示。

## 训练

```bash
python train.py \
  --data-dir ./data/own \
  --artifacts ./artifacts \
  --seq-len 24 --horizon 24 --epochs 10
```

训练输出包含模型权重和特征归一化元数据。默认预测 24 小时，并提供 0.05、0.5、0.95 分位数用于区间估计。

## 启动推理服务

```bash
python service.py --data-dir ./data/own --artifacts ./artifacts --host 127.0.0.1 --port 8090
```

接口：

- `GET /health`：服务及模型状态。
- `GET /stations`：模型可识别站点。
- `POST /predict`：输入站点、预测时刻、1/6/24 小时范围及可选历史/天气/价格，返回负荷、空闲桩和置信区间。

管理端通过 `CHARGING_ML_URL` 指定服务地址，默认 `http://127.0.0.1:8090`。服务或产物不可用时，管理端会显示失败或带标识的集中演示数据。

## 真实限制

- 天气数据需要外部 CSV 或请求参数；平台数据库本身没有天气字段。
- 预测效果依赖数据跨度和质量，课程演示指标不代表生产泛化能力。
- 模型产物、公开大数据集和 Python 虚拟环境均被视为本地运行资产，不应提交仓库。
