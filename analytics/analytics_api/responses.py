"""生成所有 Flask 接口共用的 JSON 响应格式。

输入：业务数据、状态码和提示文字，并负责转换 Decimal、日期等类型。
输出/接口：``success`` 和 ``failure``，统一包含 code、message、data、request_id、timestamp。
"""

from __future__ import annotations

from datetime import date, datetime, timezone
from decimal import Decimal

from flask import g, jsonify


def _json_value(value):
    if isinstance(value, Decimal):
        return float(value)
    if isinstance(value, (datetime, date)):
        return value.isoformat()
    if isinstance(value, dict):
        return {key: _json_value(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_value(item) for item in value]
    return value


def _envelope(code: int, message: str, data):
    return {
        "code": code,
        "message": message,
        "data": _json_value(data),
        "request_id": g.get("request_id", ""),
        "timestamp": datetime.now(timezone.utc).isoformat(timespec="seconds"),
    }


def success(data=None, status: int = 200, metadata=None):
    envelope = _envelope(0, "ok", data)
    if metadata is not None:
        envelope["metadata"] = _json_value(metadata)
    return jsonify(envelope), status


def failure(code: int, message: str, status: int, data=None):
    return jsonify(_envelope(code, message, data)), status
