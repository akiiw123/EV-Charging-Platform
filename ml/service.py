"""负荷预测 JSON API 服务（独立只读推理服务）。

按 ml/README.md 约定: 独立 Python 服务、稳定 JSON API，与 Qt GUI 解耦；
训练数据来自公开的 UrbanEV 数据集（天然脱敏），服务本身不读写业务数据库，
也不修改订单或电桩状态。

启动（在 ml 目录下）:
    python service.py --data-dir ./data --port 8090

接口:
    GET  /health     服务与模型状态
    GET  /stations   可查询站点(UrbanEV 区域)列表及总桩数
    POST /predict    负荷预测，请求体:
        {
          "station_id": 559,                      // 必填，见 /stations
          "timestamp": "2023-02-20T14:00:00",     // 可选，预测起点（默认数据集最后时刻）
          "horizons": [1, 6, 24],                 // 可选，1~horizon 内的小时数
          "recent_load_kwh": [/* 最近 seq_len 小时 */],   // 可选，默认取数据集历史
          "recent_busy_ratio": [/* 0-1 */],                // 可选
          "weather": {"T": 26, "U": 70},          // 可选，未来天气预报（缺省用最近观测持续）
          "e_price": 1.0, "s_price": 0.5          // 可选，未来电价/服务费
        }
    响应包含 1/6/24 小时的负荷(kWh)、空闲桩数量及 90% 置信区间，附完整曲线。
"""

from __future__ import annotations

import argparse
import json
import threading
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Optional
from urllib.parse import urlparse

import numpy as np
import pandas as pd
import torch

from data import (
    WEATHER_COLUMNS,
    FeatureStats,
    UrbanEVData,
    load_urbanev,
    time_features,
    zone_token,
)
from model import LoadForecaster

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_HORIZONS = (1, 6, 24)
MAX_BODY_BYTES = 1 << 20


class ForecastEngine:
    """加载训练产物并执行只读推理。"""

    def __init__(self, artifacts_dir: Path, data_dir: Optional[Path]):
        meta_path = artifacts_dir / "meta.json"
        if not meta_path.exists():
            raise FileNotFoundError(f"未找到 {meta_path}，请先运行 train.py 完成训练")
        with open(meta_path, encoding="utf-8") as fh:
            self.meta = json.load(fh)
        self.seq_len = int(self.meta["seq_len"])
        self.horizon = int(self.meta["horizon"])
        self.quantiles = np.asarray(self.meta["quantiles"], dtype=np.float64)
        self.stats = FeatureStats.from_dict(self.meta["stats"])
        self.zone_index = {zone_token(z): i for i, z in enumerate(self.meta["zones"])}
        self.pile_totals = {zone_token(k): int(v) for k, v in self.meta["pile_totals"].items()}
        self.point_index = int(np.argmin(np.abs(self.quantiles - 0.5)))

        m = self.meta["model"]
        self.model = LoadForecaster(
            past_dim=int(m["past_dim"]),
            future_dim=int(m["future_dim"]),
            horizon=self.horizon,
            num_zones=int(m["num_zones"]),
            quantiles=tuple(float(q) for q in self.quantiles),
            hidden_size=int(m["hidden_size"]),
            num_layers=int(m["num_layers"]),
            emb_dim=int(m["emb_dim"]),
            dropout=float(m["dropout"]),
        )
        checkpoint = torch.load(artifacts_dir / "model.pt", map_location="cpu", weights_only=True)
        self.model.load_state_dict(checkpoint["state_dict"])
        self.model.eval()

        self._lock = threading.Lock()
        self._data: Optional[UrbanEVData] = None
        self._data_dir = data_dir

    # ------------------------------------------------------------------ helpers
    @property
    def dataset(self) -> Optional[UrbanEVData]:
        """懒加载 UrbanEV 原始数据，用于缺省历史/天气回退。"""
        if self._data is None and self._data_dir is not None \
                and (self._data_dir / "volume.csv").exists():
            self._data = load_urbanev(self._data_dir)
        return self._data

    def _require_zone(self, station_id) -> str:
        if station_id is None:
            raise ValueError("缺少必填字段 station_id")
        zone = zone_token(station_id)
        if zone not in self.zone_index:
            raise ValueError(f"未知站点 {station_id}，可用站点见 GET /stations")
        return zone

    def _parse_horizons(self, raw) -> list:
        if raw is None:
            horizons = list(DEFAULT_HORIZONS)
        elif isinstance(raw, (list, tuple)):
            horizons = list(raw)
        else:
            raise ValueError("horizons 须为整数数组，如 [1, 6, 24]")
        if not horizons:
            raise ValueError("horizons 不能为空")
        for h in horizons:
            if not isinstance(h, int) or isinstance(h, bool) or not 1 <= h <= self.horizon:
                raise ValueError(f"horizons 取值须为 1~{self.horizon} 的整数")
        return horizons

    @staticmethod
    def _optional_float(payload: dict, key: str, default: float) -> float:
        value = payload.get(key)
        if value is None:
            return float(default)
        try:
            return float(value)
        except (TypeError, ValueError):
            raise ValueError(f"{key} 须为数值") from None

    def _origin_timestamp(self, raw) -> pd.Timestamp:
        if raw is None:
            dataset = self.dataset
            if dataset is not None:
                return dataset.times[-1]
            return pd.Timestamp.now().floor("h")
        try:
            return pd.Timestamp(raw).floor("h")
        except (ValueError, TypeError) as exc:
            raise ValueError(f"无法解析 timestamp: {raw!r}") from exc

    def _numeric_series(self, raw, name: str, length: int, clip: Optional[tuple]) -> np.ndarray:
        if not isinstance(raw, (list, tuple)):
            raise ValueError(f"{name} 须为数值数组")
        try:
            values = np.asarray(raw, dtype=np.float64)
        except (TypeError, ValueError) as exc:
            raise ValueError(f"{name} 含非数值元素") from exc
        if not np.all(np.isfinite(values)):
            raise ValueError(f"{name} 含 NaN/Inf")
        if len(values) < length:
            raise ValueError(f"{name} 至少需要 {length} 个点（seq_len={self.seq_len}）")
        values = values[-length:]
        if clip is not None:
            values = np.clip(values, clip[0], clip[1])
        return values

    def _parse_weather(self, raw) -> Optional[np.ndarray]:
        if raw is None:
            return None
        if not isinstance(raw, dict):
            raise ValueError("weather 须为对象，如 {\"T\":26,\"U\":70}")
        row = np.asarray([float(raw.get(col, np.nan)) for col in WEATHER_COLUMNS])
        if np.any(np.isnan(row)):
            missing = [c for c, v in zip(WEATHER_COLUMNS, row) if np.isnan(v)]
            raise ValueError(f"weather 缺少字段: {', '.join(missing)}")
        return row

    # ------------------------------------------------------------------ predict
    def predict(self, payload: dict) -> dict:
        if not isinstance(payload, dict):
            raise ValueError("请求体须为 JSON 对象")
        zone = self._require_zone(payload.get("station_id"))
        horizons = self._parse_horizons(payload.get("horizons"))
        timestamp = self._origin_timestamp(payload.get("timestamp"))
        L, H = self.seq_len, self.horizon
        dataset = self.dataset

        # ---- 历史: 优先请求体，缺省回退数据集
        if payload.get("recent_load_kwh") is not None:
            load_hist = self._numeric_series(payload["recent_load_kwh"], "recent_load_kwh", L, (0, None))
            if payload.get("recent_busy_ratio") is not None:
                busy_hist = self._numeric_series(payload["recent_busy_ratio"], "recent_busy_ratio", L, (0, 1))
            elif dataset is not None:
                busy_hist = self._dataset_history(zone, timestamp)[1]
            else:
                busy_hist = np.zeros(L, dtype=np.float64)
            from_dataset = False
        else:
            if dataset is None:
                raise ValueError("服务未配置数据目录，请求须携带 recent_load_kwh")
            load_hist, busy_hist = self._dataset_history(zone, timestamp)
            from_dataset = True

        # ---- 未来/历史外部特征: 天气、价格
        weather_override = self._parse_weather(payload.get("weather"))
        stats = self.stats
        if from_dataset:
            pos = self._locate(timestamp)
            zone_column = dataset.zones.index(zone)
            weather_past = dataset.weather[pos - L:pos].astype(np.float64)      # 实际观测
            e_past = dataset.e_price[pos - L:pos, zone_column].astype(np.float64)
            s_past = dataset.s_price[pos - L:pos, zone_column].astype(np.float64)
            future_weather_row = (
                weather_override if weather_override is not None
                else dataset.weather[pos - 1].astype(np.float64)
            )
            e_future = self._optional_float(
                payload, "e_price", float(dataset.e_price[pos - 1, zone_column]))
            s_future = self._optional_float(
                payload, "s_price", float(dataset.s_price[pos - 1, zone_column]))
        else:
            fallback_weather = (
                weather_override if weather_override is not None
                else stats.weather_mean.astype(np.float64)
            )
            weather_past = np.tile(fallback_weather, (L, 1))
            e_past = np.full(L, self._optional_float(payload, "e_price", stats.price_mean))
            s_past = np.full(L, self._optional_float(payload, "s_price", stats.price_mean))
            future_weather_row = fallback_weather
            e_future = self._optional_float(payload, "e_price", stats.price_mean)
            s_future = self._optional_float(payload, "s_price", stats.price_mean)

        # ---- 组装模型输入（通道顺序须与训练一致，见 data.PAST_CHANNELS/FUTURE_CHANNELS）
        tf_past = time_features(pd.date_range(timestamp - pd.Timedelta(hours=L), periods=L, freq="h"))
        tf_future = time_features(pd.date_range(timestamp, periods=H, freq="h"))
        weather_past_norm = (weather_past - stats.weather_mean) / stats.weather_std
        weather_future_norm = (np.tile(future_weather_row, (H, 1)) - stats.weather_mean) / stats.weather_std
        e_past_norm = (e_past - stats.price_mean) / stats.price_std
        s_past_norm = (s_past - stats.price_mean) / stats.price_std
        e_future_norm = np.full(H, (e_future - stats.price_mean) / stats.price_std)
        s_future_norm = np.full(H, (s_future - stats.price_mean) / stats.price_std)

        past = np.concatenate([
            stats.encode_load(load_hist)[:, None],
            busy_hist[:, None],
            e_past_norm[:, None], s_past_norm[:, None],
            weather_past_norm, tf_past,
        ], axis=1).astype(np.float32)                                   # (L, 16)
        future = np.concatenate([
            weather_future_norm, tf_future, e_future_norm[:, None], s_future_norm[:, None],
        ], axis=1).astype(np.float32)                                  # (H, 14)

        past_t = torch.from_numpy(past).unsqueeze(0)
        future_t = torch.from_numpy(future).unsqueeze(0)
        zone_t = torch.tensor([self.zone_index[zone]], dtype=torch.long)

        with self._lock, torch.no_grad():
            output = self.model(past_t, future_t, zone_t)
        quantile_out = np.sort(output[0].numpy(), axis=-1)              # (H, 2, Q) 保证单调

        load_q = stats.decode_load(quantile_out[:, 0, :])               # (H, Q) kWh
        busy_q = np.clip(quantile_out[:, 1, :], 0.0, 1.0)               # (H, Q)
        total_piles = self.pile_totals.get(zone, 0)

        def load_entry(h: int) -> dict:
            values = load_q[h - 1]
            return {"point": float(values[self.point_index]),
                    "lower": float(values[0]), "upper": float(values[-1])}

        def piles_entry(h: int) -> dict:
            values = busy_q[h - 1]
            point = int(np.clip(round(total_piles * (1 - values[self.point_index])), 0, total_piles))
            lower = int(np.clip(round(total_piles * (1 - values[-1])), 0, total_piles))
            upper = int(np.clip(round(total_piles * (1 - values[0])), 0, total_piles))
            return {"point": point, "lower": min(lower, point), "upper": max(upper, point)}

        curve = []
        for offset in range(1, H + 1):
            values_load, values_busy = load_q[offset - 1], busy_q[offset - 1]
            curve.append({
                "offset": offset,
                "timestamp": (timestamp + pd.Timedelta(hours=offset)).isoformat(),
                "load_kwh": float(values_load[self.point_index]),
                "load_lower": float(values_load[0]),
                "load_upper": float(values_load[-1]),
                "busy_ratio": float(values_busy[self.point_index]),
            })

        return {
            "station_id": zone,
            "timestamp": timestamp.isoformat(),
            "generated_at": datetime.now().isoformat(timespec="seconds"),
            "total_piles": total_piles,
            "horizons": horizons,
            "quantiles": [float(q) for q in self.quantiles],
            "load_kwh": {str(h): load_entry(h) for h in horizons},
            "available_piles": {str(h): piles_entry(h) for h in horizons},
            "curve": curve,
        }

    def _dataset_history(self, zone: str, timestamp: pd.Timestamp) -> tuple:
        dataset = self.dataset
        if dataset is None:
            raise ValueError("服务未配置数据目录，无法回退历史数据")
        pos = self._locate(timestamp)
        z = dataset.zones.index(zone)
        return (dataset.volume[pos - self.seq_len:pos, z].astype(np.float64),
                dataset.busy_ratio[pos - self.seq_len:pos, z].astype(np.float64))

    def _locate(self, timestamp: pd.Timestamp) -> int:
        times = self.dataset.times
        pos = int(times.searchsorted(timestamp))
        if pos < len(times) and times[pos] == timestamp:
            return pos
        if pos == 0:
            raise ValueError(f"timestamp {timestamp} 早于数据集起点 {times[0]}")
        return pos - 1                                                # 向前对齐最近一小时


class ForecastHandler(BaseHTTPRequestHandler):
    engine: ForecastEngine
    server_version = "EVForecast/1.0"

    def send_json(self, payload: dict, status: int = 200) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        path = urlparse(self.path).path
        if path == "/health":
            engine = self.engine
            self.send_json({
                "status": "ok",
                "model": "urbanev-load-forecaster",
                "zones": len(engine.zone_index),
                "horizon_hours": engine.horizon,
                "quantiles": [float(q) for q in engine.quantiles],
                "dataset_available": engine.dataset is not None,
                "data_range": engine.meta.get("data_range"),
            })
        elif path == "/stations":
            stations = [
                {"station_id": zone, "total_piles": piles}
                for zone, piles in sorted(
                    self.engine.pile_totals.items(), key=lambda kv: int(kv[0])
                )
            ]
            self.send_json({"count": len(stations), "stations": stations})
        elif path == "/":
            self.send_json({
                "service": "EV 负荷预测服务",
                "endpoints": ["GET /health", "GET /stations", "POST /predict"],
            })
        else:
            self.send_json({"error": f"未知路径 {path}"}, status=404)

    def do_POST(self) -> None:
        if urlparse(self.path).path != "/predict":
            self.send_json({"error": "仅支持 POST /predict"}, status=404)
            return
        try:
            length = int(self.headers.get("Content-Length", 0))
        except ValueError:
            self.send_json({"error": "非法的 Content-Length"}, status=400)
            return
        if length <= 0:
            self.send_json({"error": "请求体不能为空"}, status=400)
            return
        if length > MAX_BODY_BYTES:
            self.send_json({"error": "请求体过大"}, status=413)
            return
        try:
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            self.send_json(self.engine.predict(payload))
        except (ValueError, KeyError) as exc:
            self.send_json({"error": str(exc)}, status=400)
        except Exception as exc:  # 兜底: 不让线程崩溃
            self.send_json({"error": f"内部错误: {exc}"}, status=500)

    def log_message(self, fmt: str, *args) -> None:
        print(f"[{datetime.now():%H:%M:%S}] {self.address_string()} {fmt % args}")


def main() -> None:
    parser = argparse.ArgumentParser(description="EV 负荷预测 JSON API 服务")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8090)
    parser.add_argument("--artifacts", type=Path, default=SCRIPT_DIR / "artifacts")
    parser.add_argument("--data-dir", type=Path, default=SCRIPT_DIR / "data",
                        help="UrbanEV data 目录（用于缺省历史回退，可不存在）")
    args = parser.parse_args()

    engine = ForecastEngine(args.artifacts, args.data_dir)
    ForecastHandler.engine = engine
    server = ThreadingHTTPServer((args.host, args.port), ForecastHandler)
    print(f"负荷预测服务: http://{args.host}:{args.port}  "
          f"站点数={len(engine.zone_index)}  模型={args.artifacts / 'model.pt'}")
    print("接口: GET /health  GET /stations  POST /predict")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n服务已停止")


if __name__ == "__main__":
    main()
