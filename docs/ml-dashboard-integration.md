# ML 历史预测接入大屏

## 服务关系

```text
Vue 大屏
  -> /api/v1/ml/*
Flask 分析服务（8091）
  -> http://127.0.0.1:8090
ML 历史预测服务（8090）
  -> ml/artifacts/sessions/model.joblib
```

浏览器不直接访问 8090。Flask 负责同源代理、超时和服务不可用响应。

## 部署模型产物

从可信的“ML-年份调整后最终交付”复制 `ml/artifacts/sessions/` 到服务器仓库。该目录已被 `.gitignore` 排除，不提交 Git。

```bash
cd /home/bit/EV-Charging-Platform
python3.12 -m venv .venv-ml
.venv-ml/bin/python -m pip install -r ml/requirements-sessions.txt
.venv-ml/bin/python ml/smoke_sessions.py --artifacts ml/artifacts/sessions
```

冒烟测试应报告 76 个可回放站点和默认时间 `2025-09-03T00:00:00`。

## 启动

临时运行：

```bash
.venv-ml/bin/python ml/service.py \
  --backend sessions \
  --artifacts ml/artifacts/sessions \
  --host 127.0.0.1 \
  --port 8090
```

长期运行可安装 `ml/deploy/charging-ml.service`。Flask 环境增加：

```dotenv
ML_SERVICE_URL=http://127.0.0.1:8090
ML_SERVICE_TIMEOUT_SECONDS=5
```

## 验收

```bash
curl http://127.0.0.1:8091/api/v1/ml/health
curl http://127.0.0.1:8091/api/v1/ml/stations
curl -X POST http://127.0.0.1:8091/api/v1/ml/predict \
  -H 'Content-Type: application/json' \
  --data '{"station_id":"129465","horizons":[1,6,24]}'
```

页面必须遵守以下口径：

- 标记为 2025 年历史回放，不称为实时预测。
- 显示模型实际选择的 `seasonal`，不称为深度学习结果。
- 8090 不可用时明确显示历史统计估算和不可用原因。
- 站点列表成功但推理失败时清空旧模型结果，不沿用上次成功值。
- 数据集站点 ID 不与业务数据库站点 ID 自动绑定。
