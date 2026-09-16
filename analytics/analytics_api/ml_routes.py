"""把独立 8090 预测服务代理到大屏同源 API。

输入：``ML_SERVICE_URL`` 指向只读模型服务，请求体沿用 ML 的预测协议。
输出/接口：``/api/v1/ml/health``、``/stations``、``/predict``；模型不可用时返回 503。
"""

from __future__ import annotations

import json
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

from flask import Blueprint, current_app, jsonify, request


ml_api = Blueprint("ml_api", __name__)


def _proxy(path: str, method: str = "GET", payload: dict | None = None):
    body = None if payload is None else json.dumps(payload, ensure_ascii=False).encode("utf-8")
    upstream = Request(
        current_app.config["ML_SERVICE_URL"] + path,
        data=body,
        method=method,
        headers={"Accept": "application/json", "Content-Type": "application/json"},
    )
    try:
        with urlopen(
            upstream, timeout=current_app.config["ML_SERVICE_TIMEOUT_SECONDS"]
        ) as response:
            data = json.loads(response.read().decode("utf-8"))
            return jsonify(data), response.status
    except HTTPError as error:
        try:
            data = json.loads(error.read().decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            data = {"error": "模型服务返回了无法解析的错误"}
        return jsonify(data), error.code
    except (URLError, TimeoutError, OSError, json.JSONDecodeError) as error:
        current_app.logger.warning("ML service unavailable: %s", error)
        return jsonify({
            "error": "模型服务不可用",
            "reason": type(error).__name__,
            "source": "ml-service",
        }), 503


@ml_api.get("/ml/health")
def ml_health():
    return _proxy("/health")


@ml_api.get("/ml/stations")
def ml_stations():
    return _proxy("/stations")


@ml_api.post("/ml/predict")
def ml_predict():
    payload = request.get_json(silent=True)
    if not isinstance(payload, dict):
        return jsonify({"error": "请求体须为 JSON 对象"}), 400
    return _proxy("/predict", "POST", payload)
