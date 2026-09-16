"""提供 ADS 文件的统一字段契约、CSV 解析、校验和计算与完整性检查。

输入：``ads_contract.json``、十个竖线分隔 CSV 和 manifest.json。
输出/接口：TABLES/CONTRACT 以及 ``validate_export``，不依赖 Spark 或 MySQL 也能预检交付物。
"""
from __future__ import annotations

import csv
import hashlib
import json
import re
import io
from decimal import Decimal, InvalidOperation, ROUND_HALF_UP
from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = json.loads((ROOT / "ads_contract.json").read_text(encoding="utf-8"))
TABLES = CONTRACT["tables"]


def convert(value: str, kind: str, nullable: bool):
    if value == "":
        if nullable:
            return None
        if not kind.startswith("varchar"):
            raise ValueError("required numeric value is empty")
    if kind.startswith("varchar"):
        maximum = int(re.search(r"\((\d+)\)", kind).group(1))
        if not value or len(value) > maximum or "\x00" in value:
            raise ValueError("invalid or oversized text")
        return value
    try:
        number = Decimal(value)
    except InvalidOperation as exc:
        raise ValueError("invalid number") from exc
    if not number.is_finite():
        raise ValueError("non-finite number")
    if kind.startswith("decimal"):
        precision, scale = map(int, re.findall(r"\d+", kind))
        if abs(number) >= Decimal(10) ** (precision - scale):
            raise ValueError("decimal overflow")
        if number != number.quantize(Decimal(10) ** -scale):
            raise ValueError("decimal scale exceeds contract")
        return number
    bits = {"tinyint": 8, "int": 32, "bigint": 64}[kind]
    if number != number.to_integral_value() or not -(2 ** (bits-1)) <= number < 2 ** (bits-1):
        raise ValueError("integer overflow or fractional integer")
    return int(number)


def read_table(directory: Path, table: str, expected_hash: str | None = None):
    spec = TABLES[table]
    names = [column[0] for column in spec["columns"]]
    rows, keys = [], set()
    content = (directory / f"{table}.csv").read_bytes()
    if expected_hash is not None and hashlib.sha256(content).hexdigest() != expected_hash:
        raise ValueError(f"{table}: checksum mismatch")
    with io.StringIO(content.decode("utf-8"), newline="") as stream:
        for line, values in enumerate(csv.reader(stream, delimiter="|", strict=True), 1):
            if len(values) != len(names):
                raise ValueError(f"{table} row {line}: wrong column count")
            try:
                row = tuple(convert(value, kind, name in spec.get("nullable", []))
                            for value, (name, kind) in zip(values, spec["columns"]))
            except ValueError as exc:
                raise ValueError(f"{table} row {line}: {exc}") from exc
            if spec.get("key"):
                key = tuple(row[names.index(name)] for name in spec["key"])
                if key in keys:
                    raise ValueError(f"{table} row {line}: duplicate primary key")
                keys.add(key)
            data = dict(zip(names, row))
            for name in ("sessions", "user_count", "sess_count", "station_count", "total_sessions", "total_kwh", "total_fee", "revenue", "cost", "daily_kwh", "avg_fee_per_kwh"):
                if name in data and data[name] is not None and data[name] < 0:
                    raise ValueError(f"{table}: {name} cannot be negative")
            for name in ("abnormal_rate", "dim_value", "utilization_rate", "pct", "ratio"):
                if name in data and data[name] is not None and not 0 <= data[name] <= 100:
                    raise ValueError(f"{table}: {name} must be between 0 and 100")
            if table == "ads_hour_trend" and (not 0 <= data["hour"] <= 23 or data["is_peak"] not in (0, 1)):
                raise ValueError("hour trend contains invalid hour or peak flag")
            rows.append(row)
    if not spec.get("min_rows", 0) <= len(rows) <= spec.get("max_rows", 100000):
        raise ValueError(f"{table}: invalid row count {len(rows)}")
    return rows


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_export(directory: Path):
    manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("contract_version") != CONTRACT["version"]:
        raise ValueError("unsupported contract version")
    analysis = manifest.get("analysis", {})
    if analysis.get("module") != "analytics/scripts/evcharging_analysis.py" or not re.fullmatch(r"[a-f0-9]{64}", analysis.get("script_sha256", "")):
        raise ValueError("missing unified analysis source identity")
    results = {}
    for table in TABLES:
        expected = manifest["tables"][table]
        results[table] = read_table(directory, table, expected["sha256"])
        if len(results[table]) != expected["rows"]:
            raise ValueError(f"{table}: manifest row count mismatch")
    datetime.fromisoformat(manifest["generated_at"])
    quality = manifest["quality"]
    for name in ("raw_count","valid_count","rejected_count","platform_rows","soc_rows"):
        if type(quality.get(name)) is not int or quality[name] < 0:
            raise ValueError(f"invalid quality counter {name}")
    if quality["raw_count"] == 0 or quality["valid_count"] == 0 or quality["valid_count"] + quality["rejected_count"] != quality["raw_count"]:
        raise ValueError("inconsistent source quality counters")
    kpi = results["ads_kpi_overview"][0]
    expected_rate = (Decimal(quality["rejected_count"]) / quality["raw_count"] * 100).quantize(Decimal("0.001"), rounding=ROUND_HALF_UP)
    if kpi[4] != expected_rate:
        raise ValueError("rejected order rate does not match source counters")
    hours, weeks = results["ads_hour_trend"], results["ads_week_compare"]
    if {row[0] for row in hours} != set(range(24)) or {row[0] for row in weeks} != {"工作日","周末"}:
        raise ValueError("missing hour/day categories")
    for rows in (hours,weeks):
        if sum(row[1] for row in rows) != kpi[0] or sum(row[2] for row in rows) != kpi[1]:
            raise ValueError("hour/week totals do not match KPI")
    if sum(row[3] for row in results["ads_station_topn"]) > kpi[0]:
        raise ValueError("station TOP totals exceed KPI")
    if sum(row[1] for row in results["ads_battery_health"]) != quality["soc_rows"]:
        raise ValueError("SOC totals do not match available source rows")
    if bool(results["ads_platform_dist"]) != bool(quality["platform_rows"]):
        raise ValueError("platform availability does not match source")
    if max(quality["soc_rows"],quality["platform_rows"]) > kpi[0]:
        raise ValueError("optional source counts exceed session count")
    return manifest, results
