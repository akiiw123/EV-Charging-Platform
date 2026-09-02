"""UrbanEV 数据集加载与特征工程（仅依赖 numpy/pandas）。

数据集: https://github.com/IntelligentSystemsLab/UrbanEV
所需文件（放在 data-dir 下）:
    volume.csv       时间 x 区域(TAZID) 每小时充电量 kWh（负荷）
    occupancy.csv    时间 x 区域 占用情况（计数/百分比口径自动识别）
    e_price.csv      时间 x 区域 电价 元/kWh
    s_price.csv      时间 x 区域 服务费 元/kWh
    weather_central.csv  时间 x 气象列(T,P0,P,U,nRAIN,Td)
    inf.csv          站点信息（station_id, TAZID, charge_count, ...）

自有平台数据可经 export.py 导出为同布局目录后直接复用本模块
（站点 ID 即 charging_stations.id），训练与推理代码无需改动。

样本定义: 给定站点过去 seq_len 小时特征，预测未来 horizon 小时的
负荷（充电量 kWh）与忙桩比例；忙桩比例 x 总桩数 = 被占用桩数。
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import date
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np
import pandas as pd

WEATHER_COLUMNS: Tuple[str, ...] = ("T", "P0", "P", "U", "nRAIN", "Td")

# 数据集覆盖范围(2022-09~2023-02)内的中国法定节假日；调休上班日仍视为工作日
HOLIDAYS = frozenset(map(date.fromisoformat, [
    "2022-09-10", "2022-09-11", "2022-09-12",                        # 中秋
    "2022-10-01", "2022-10-02", "2022-10-03", "2022-10-04",
    "2022-10-05", "2022-10-06", "2022-10-07",                        # 国庆
    "2022-12-31",
    "2023-01-01", "2023-01-02",                                      # 元旦
    "2023-01-21", "2023-01-22", "2023-01-23", "2023-01-24",
    "2023-01-25", "2023-01-26", "2023-01-27",                        # 春节
]))

# past 通道顺序（编码器输入，共 16 维）
PAST_CHANNELS = (
    ["load_norm", "busy_ratio", "e_price_norm", "s_price_norm"]
    + [f"weather_{c}" for c in WEATHER_COLUMNS]
    + ["hour_sin", "hour_cos", "dow_sin", "dow_cos", "is_weekend", "is_holiday"]
)
# future 通道顺序（解码器输入，共 14 维；未来可知：天气预报/时间/价格）
FUTURE_CHANNELS = (
    [f"weather_{c}" for c in WEATHER_COLUMNS]
    + ["hour_sin", "hour_cos", "dow_sin", "dow_cos", "is_weekend", "is_holiday"]
    + ["e_price_norm", "s_price_norm"]
)
# 预测目标通道顺序
TARGET_CHANNELS = ["load_norm", "busy_ratio"]


def zone_token(value) -> str:
    """把区域(TAZID)/站点 ID 统一为干净的字符串，如 552.0 -> '552'。"""
    text = str(value).strip()
    if text.endswith(".0"):
        text = text[:-2]
    return text


def time_features(index: pd.DatetimeIndex) -> np.ndarray:
    """生成 (T, 6) 时间特征: hour/dow 循环编码 + 周末 + 节假日标记。"""
    hour = index.hour.to_numpy(dtype=np.float64)
    dow = index.dayofweek.to_numpy(dtype=np.float64)
    feats = np.stack([
        np.sin(2 * np.pi * hour / 24.0),
        np.cos(2 * np.pi * hour / 24.0),
        np.sin(2 * np.pi * dow / 7.0),
        np.cos(2 * np.pi * dow / 7.0),
        (dow >= 5).astype(np.float64),
        np.array([1.0 if ts.date() in HOLIDAYS else 0.0 for ts in index]),
    ], axis=1)
    return feats.astype(np.float32)


@dataclass
class UrbanEVData:
    """整理后的 zone 级小时数据。"""
    times: pd.DatetimeIndex
    zones: List[str]
    pile_totals: Dict[str, int]   # 区域 -> 总充电桩数
    volume: np.ndarray            # (T, Z) 每小时充电量 kWh
    busy_ratio: np.ndarray        # (T, Z) 忙桩比例 0-1
    e_price: np.ndarray           # (T, Z) 元/kWh
    s_price: np.ndarray           # (T, Z) 元/kWh
    weather: np.ndarray           # (T, 6) 市区气象站逐小时观测


@dataclass
class FeatureStats:
    """仅用训练段拟合的归一化统计量，随模型一起保存供推理反变换。"""
    load_mean: float
    load_std: float
    price_mean: float
    price_std: float
    weather_mean: np.ndarray      # (6,)
    weather_std: np.ndarray

    def encode_load(self, volume: np.ndarray) -> np.ndarray:
        return (np.log1p(np.clip(volume, 0, None)) - self.load_mean) / self.load_std

    def decode_load(self, normalized: np.ndarray) -> np.ndarray:
        value = np.expm1(normalized * self.load_std + self.load_mean)
        return np.clip(value, 0.0, None)

    def to_dict(self) -> dict:
        return {
            "load_mean": self.load_mean, "load_std": self.load_std,
            "price_mean": self.price_mean, "price_std": self.price_std,
            "weather_mean": self.weather_mean.tolist(),
            "weather_std": self.weather_std.tolist(),
        }

    @classmethod
    def from_dict(cls, payload: dict) -> "FeatureStats":
        return cls(
            load_mean=float(payload["load_mean"]), load_std=float(payload["load_std"]),
            price_mean=float(payload["price_mean"]), price_std=float(payload["price_std"]),
            weather_mean=np.asarray(payload["weather_mean"], dtype=np.float64),
            weather_std=np.asarray(payload["weather_std"], dtype=np.float64),
        )


def to_busy_ratio(values: np.ndarray, total_piles: int) -> np.ndarray:
    """把 occupancy 原始值稳健地换算为 0-1 忙桩比例。

    不同版本数据存在 比例(0-1)/忙桩计数/百分比 三种口径，按取值特征自动识别。
    """
    values = np.nan_to_num(np.asarray(values, dtype=np.float64), nan=0.0)
    upper = float(values.max()) if values.size else 0.0
    if upper <= 1.0 + 1e-6:
        ratio = values                                    # 已是比例
    elif total_piles > 0 and np.allclose(values, np.round(values)) and upper <= total_piles + 1e-6:
        ratio = values / float(total_piles)               # 忙桩计数
    else:
        ratio = values / 100.0                            # 百分比
    return np.clip(ratio, 0.0, 1.0).astype(np.float32)


def _read_wide_csv(path: Path) -> pd.DataFrame:
    frame = pd.read_csv(path, index_col=0)
    frame.index = pd.to_datetime(frame.index)
    frame.columns = [zone_token(c) for c in frame.columns]
    return frame.sort_index()


def load_urbanev(data_dir: Path) -> UrbanEVData:
    """读取并按时间/区域对齐全部 zone 级数据。"""
    data_dir = Path(data_dir)
    inf = pd.read_csv(data_dir / "inf.csv")
    pile_totals = {
        zone_token(z): int(c)
        for z, c in inf.groupby(inf["TAZID"].map(zone_token))["charge_count"].sum().items()
    }

    volume = _read_wide_csv(data_dir / "volume.csv")
    zones = sorted(volume.columns.tolist())
    times = volume.index

    def align(name: str) -> pd.DataFrame:
        frame = _read_wide_csv(data_dir / name)
        frame = frame.reindex(times).reindex(columns=zones)
        return frame.ffill().bfill()

    occupancy = align("occupancy.csv")
    e_price = align("e_price.csv")
    s_price = align("s_price.csv")

    weather = pd.read_csv(data_dir / "weather_central.csv", index_col="time")
    weather.index = pd.to_datetime(weather.index)
    weather = weather[list(WEATHER_COLUMNS)].sort_index()
    weather = weather[~weather.index.duplicated(keep="last")]
    weather = weather.reindex(times, method="ffill").bfill()

    busy = np.zeros((len(times), len(zones)), dtype=np.float32)
    for j, zone in enumerate(zones):
        busy[:, j] = to_busy_ratio(occupancy[zone].to_numpy(), pile_totals.get(zone, 0))

    return UrbanEVData(
        times=times,
        zones=zones,
        pile_totals=pile_totals,
        volume=np.nan_to_num(volume.to_numpy(dtype=np.float32)),
        busy_ratio=busy,
        e_price=np.nan_to_num(e_price.to_numpy(dtype=np.float32)),
        s_price=np.nan_to_num(s_price.to_numpy(dtype=np.float32)),
        weather=np.nan_to_num(weather.to_numpy(dtype=np.float32)),
    )


def build_features(
    data: UrbanEVData, train_end: int
) -> Tuple[np.ndarray, np.ndarray, np.ndarray, FeatureStats]:
    """构建 past (T,Z,16) / future (T,Z,14) / targets (T,Z,2) 特征数组。

    归一化统计量只用 [0, train_end) 拟合，避免测试集信息泄露。
    """
    train_volume = data.volume[:train_end]
    log_load = np.log1p(np.clip(train_volume, 0, None))
    load_mean = float(log_load.mean())
    load_std = float(log_load.std() + 1e-6)

    prices = np.concatenate([data.e_price[:train_end].ravel(), data.s_price[:train_end].ravel()])
    price_mean = float(prices.mean())
    price_std = float(prices.std() + 1e-6)

    weather_mean = data.weather[:train_end].mean(axis=0)
    weather_std = data.weather[:train_end].std(axis=0) + 1e-6

    stats = FeatureStats(load_mean, load_std, price_mean, price_std, weather_mean, weather_std)

    T, Z = data.volume.shape
    tf = time_features(data.times)                                    # (T, 6)
    weather_norm = ((data.weather - weather_mean) / weather_std).astype(np.float32)
    e_norm = (((data.e_price - price_mean) / price_std)[..., None]).astype(np.float32)   # (T, Z, 1)
    s_norm = (((data.s_price - price_mean) / price_std)[..., None]).astype(np.float32)
    load_norm = stats.encode_load(data.volume).astype(np.float32)     # (T, Z)
    busy = data.busy_ratio

    weather_b = np.broadcast_to(weather_norm[:, None, :], (T, Z, len(WEATHER_COLUMNS)))
    tf_b = np.broadcast_to(tf[:, None, :], (T, Z, 6))

    past = np.concatenate(
        [load_norm[..., None], busy[..., None], e_norm, s_norm, weather_b, tf_b],
        axis=2,
    ).astype(np.float32)                                              # (T, Z, 16)
    future = np.concatenate(
        [weather_b, tf_b, e_norm, s_norm],
        axis=2,
    ).astype(np.float32)                                              # (T, Z, 14)
    targets = np.stack([load_norm, busy], axis=2).astype(np.float32)  # (T, Z, 2)
    return past, future, targets, stats


class ForecastDataset:
    """按 (zone, t) 滑窗取样: past=[t-L, t), future/target=[t, t+H)。

    region=(start, end) 限定窗口完全落在指定时间段内，保证训练/验证/测试不泄露。
    返回 numpy 数组，可直接被 torch DataLoader 的默认 collate 转为张量。
    """

    def __init__(
        self,
        past: np.ndarray,
        future: np.ndarray,
        targets: np.ndarray,
        seq_len: int,
        horizon: int,
        region: Tuple[int, int],
        stride: int = 1,
    ):
        self.past = past
        self.future = future
        self.targets = targets
        self.seq_len = seq_len
        self.horizon = horizon
        start, end = region
        self.samples: List[Tuple[int, int]] = []
        for t in range(start + seq_len, end - horizon + 1, stride):
            for z in range(past.shape[1]):
                self.samples.append((z, t))

    def __len__(self) -> int:
        return len(self.samples)

    def __getitem__(self, index: int) -> dict:
        z, t = self.samples[index]
        return {
            "past": self.past[t - self.seq_len:t, z],
            "future": self.future[t:t + self.horizon, z],
            "target": self.targets[t:t + self.horizon, z],
            "zone": np.int64(z),
        }
