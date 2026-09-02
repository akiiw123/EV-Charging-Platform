"""把平台 SQLite 运营数据导出为 ml 模块的标准训练数据格式。

标准格式与 UrbanEV zone 级 CSV 布局完全一致（由 data.load_urbanev 加载）:
    volume.csv           时间 x 站点  每小时充电量 kWh
    occupancy.csv        时间 x 站点  忙桩比例 0-1
    e_price.csv          时间 x 站点  电价 元/kWh（取 charging_stations.price_per_kwh）
    s_price.csv          时间 x 站点  服务费 元/kWh（平台暂无此字段，填 0）
    weather_central.csv  时间 x 气象列（暂无采集则常数填充，--weather-csv 可换实测）
    inf.csv              站点信息（charge_count=站点桩数，TAZID=站点ID）

导出后训练与推理代码零改动，站点 ID 即 charging_stations.id:
    python export.py --db ../database/your.db --out ./data/own
    python train.py --data-dir ./data/own --artifacts ./artifacts
    python service.py --data-dir ./data/own --artifacts ./artifacts

统计口径:
    负荷  每笔订单的 energy_kwh 按时间重叠比例分摊到所跨小时；
          进行中的订单（status='charging' 且 ended_at 为空）分摊至当前时刻。
    占用  每小时被订单占用的桩·小时数 / 站点总桩数
          （计入 started_at 非空且时间有效的订单）。

脱敏与安全: 仅导出站点级小时聚合，不含任何用户字段；数据库以只读方式打开，
不修改订单或电桩状态（满足 ml/README.md 约束）。
"""

from __future__ import annotations

import argparse
import sqlite3
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np
import pandas as pd

SCRIPT_DIR = Path(__file__).resolve().parent

WEATHER_COLUMNS = ("T", "P0", "P", "U", "nRAIN", "Td")
# 平台暂无气象采集时的常数填充（之后可用 --weather-csv 提供实测或接天气 API）
DEFAULT_WEATHER = (25.0, 750.0, 756.0, 70.0, 0.0, 20.0)
HOUR = pd.Timedelta(hours=1)
MIN_RECOMMENDED_HOURS = 24 * 28    # 建议至少积累 4 周数据再训练自有模型


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="导出平台运营数据为标准训练格式")
    parser.add_argument("--db", type=Path, required=True, help="平台 SQLite 数据库路径")
    parser.add_argument("--out", type=Path, default=SCRIPT_DIR / "data" / "own",
                        help="导出目录（可直接作为 train.py 的 --data-dir）")
    parser.add_argument("--weather-csv", type=Path, default=None,
                        help="实测天气 CSV（列: time,T,P0,P,U,nRAIN,Td），缺省常数填充")
    return parser.parse_args()


def read_database(db_path: Path):
    if not db_path.exists():
        raise SystemExit(f"数据库不存在: {db_path}")
    uri = db_path.resolve().as_uri() + "?mode=ro"    # 只读连接，不触碰业务数据
    connection = sqlite3.connect(uri, uri=True)
    try:
        stations = pd.read_sql_query(
            "SELECT id, longitude, latitude, price_per_kwh FROM charging_stations ORDER BY id",
            connection)
        piles = pd.read_sql_query("SELECT id, station_id FROM charging_piles", connection)
        orders = pd.read_sql_query(
            "SELECT pile_id, status, started_at, ended_at, energy_kwh FROM charging_orders"
            " WHERE started_at IS NOT NULL", connection)
    finally:
        connection.close()
    return stations, piles, orders


def build_sessions(orders: pd.DataFrame, pile_station: Dict[int, int],
                   station_ids: set, now: pd.Timestamp) -> Tuple[List[tuple], int]:
    """把订单行转成 (station_id, start, end, energy) 会话列表。"""
    starts = pd.to_datetime(orders["started_at"], errors="coerce")
    ends = pd.to_datetime(orders["ended_at"], errors="coerce")
    energies = orders["energy_kwh"].fillna(0.0).astype(float)
    sessions, skipped = [], 0
    for pile_id, status, start, end, energy in zip(
            orders["pile_id"], orders["status"], starts, ends, energies):
        station = pile_station.get(pile_id)
        if pd.isna(start) or station is None or station not in station_ids:
            skipped += 1
            continue
        if pd.isna(end):                       # 进行中的订单分摊至当前时刻
            end = now if status == "charging" else pd.NaT
        if pd.isna(end) or end <= start:
            skipped += 1
            continue
        sessions.append((int(station), start, end, float(energy)))
    return sessions, skipped


def aggregate(sessions, zone_ids, pile_counts, t0, n_hours):
    """按小时桶聚合负荷(kWh)与忙桩比例。"""
    station_index = {z: j for j, z in enumerate(zone_ids)}
    volume = np.zeros((n_hours, len(zone_ids)))
    busy = np.zeros((n_hours, len(zone_ids)))
    for station, start, end, energy in sessions:
        j = station_index[str(station)]
        duration = (end - start).total_seconds()
        cur = start.floor("h")
        while cur < end:
            overlap = (min(end, cur + HOUR) - max(start, cur)).total_seconds()
            if overlap > 0:
                i = int((cur - t0) / HOUR)
                volume[i, j] += energy * overlap / duration
                busy[i, j] += overlap / 3600.0
            cur = cur + HOUR
    counts = np.array([pile_counts.get(int(z), 0) for z in zone_ids], dtype=float)
    ratio = busy / np.maximum(counts, 1.0)[None, :]
    return volume, np.clip(ratio, 0.0, 1.0)


def build_weather(grid: pd.DatetimeIndex, weather_csv) -> pd.DataFrame:
    if weather_csv is None:
        return pd.DataFrame([DEFAULT_WEATHER] * len(grid), index=grid,
                             columns=list(WEATHER_COLUMNS))
    raw = pd.read_csv(weather_csv)
    if "time" not in raw.columns:
        raise SystemExit("weather-csv 缺少 time 列")
    raw.index = pd.to_datetime(raw.pop("time"), errors="coerce")
    raw = raw[list(WEATHER_COLUMNS)].dropna(axis=0, how="any").sort_index()
    raw = raw[~raw.index.duplicated(keep="last")]
    weather = raw.reindex(grid).ffill().bfill()
    if weather.isna().any().any():
        raise SystemExit("--weather-csv 数据无法覆盖导出的时间范围")
    return weather


def main() -> None:
    args = parse_args()
    stations, piles, orders = read_database(args.db)
    if stations.empty:
        raise SystemExit("charging_stations 为空，没有可导出的站点")
    if orders.empty:
        raise SystemExit("没有 started_at 非空的订单，暂无可导出的训练数据")

    pile_station = dict(zip(piles["id"], piles["station_id"]))
    pile_counts = piles.groupby("station_id").size().to_dict()
    zone_ids = [str(s) for s in stations["id"]]

    now = pd.Timestamp.now().floor("h")
    sessions, skipped = build_sessions(orders, pile_station, set(stations["id"]), now)
    if not sessions:
        raise SystemExit("有效订单为 0（时间字段无法解析、状态异常或电桩不匹配）")

    t0 = min(s[1] for s in sessions).floor("h")
    t1 = max(s[2] for s in sessions).floor("h")
    grid = pd.date_range(t0, t1, freq="h")
    volume, busy = aggregate(sessions, zone_ids, pile_counts, t0, len(grid))

    args.out.mkdir(parents=True, exist_ok=True)
    index = pd.Index(grid.strftime("%Y-%m-%d %H:%M"))
    pd.DataFrame(volume, index=index, columns=zone_ids).to_csv(args.out / "volume.csv")
    pd.DataFrame(busy, index=index, columns=zone_ids).to_csv(args.out / "occupancy.csv")

    prices = stations["price_per_kwh"].to_numpy(dtype=float)
    pd.DataFrame(np.tile(prices, (len(grid), 1)), index=index, columns=zone_ids
                 ).to_csv(args.out / "e_price.csv")
    pd.DataFrame(np.zeros((len(grid), len(zone_ids))), index=index, columns=zone_ids
                 ).to_csv(args.out / "s_price.csv")

    weather = build_weather(grid, args.weather_csv)
    weather.index = pd.Index(grid.strftime("%Y-%m-%d %H:%M"), name="time")
    weather.to_csv(args.out / "weather_central.csv")

    pd.DataFrame({
        "station_id": stations["id"].to_numpy(),
        "longitude": stations["longitude"].to_numpy(),
        "latitude": stations["latitude"].to_numpy(),
        "charge_count": [pile_counts.get(int(z), 0) for z in zone_ids],
        "TAZID": stations["id"].to_numpy(),
        "area": 0.0,
        "perimeter": 0.0,
    }).to_csv(args.out / "inf.csv", index=False)

    print(f"导出完成: {len(grid)} 小时 ({t0} ~ {t1}) x {len(zone_ids)} 站点，"
          f"有效订单 {len(sessions)} 条（跳过 {skipped} 条）")
    print(f"负荷合计 {volume.sum():.1f} kWh，平均忙桩比例 {busy.mean():.2%}")
    if len(grid) < MIN_RECOMMENDED_HOURS:
        print(f"提示: 当前仅 {len(grid)} 小时数据，建议积累 {MIN_RECOMMENDED_HOURS // 24} 天以上"
              "再训练自有模型，期间可继续使用 UrbanEV 模型")
    print("下一步:")
    print(f"    python train.py --data-dir {args.out} --artifacts <产物目录>")
    print(f"    python service.py --data-dir {args.out} --artifacts <产物目录>")


if __name__ == "__main__":
    main()
