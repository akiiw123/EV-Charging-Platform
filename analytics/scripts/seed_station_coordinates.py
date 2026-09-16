"""Generate deterministic display coordinates for historical stations without GPS data."""
from __future__ import annotations

import argparse
import csv
import hashlib
import math
import os
import re
from pathlib import Path


ADDRESS_ANCHORS = {
    "河南省郑州市上街区中心路": (34.8080, 113.2970),
    "河南省郑州市中原区建设路": (34.7485, 113.6175),
    "河南省郑州市中原区桐柏路": (34.7465, 113.6060),
    "河南省郑州市中原区西三环": (34.7440, 113.5700),
    "河南省郑州市二七区大学路": (34.7185, 113.6410),
    "河南省郑州市二七区嵩山路": (34.7205, 113.6260),
    "河南省郑州市二七区淮河路": (34.7255, 113.6360),
    "河南省郑州市惠济区天河路": (34.8890, 113.6040),
    "河南省郑州市惠济区文化路": (34.8680, 113.6680),
    "河南省郑州市管城区紫荆山路": (34.7350, 113.6810),
    "河南省郑州市管城区陇海路": (34.7290, 113.6770),
    "河南省郑州市经开区第八大街": (34.7170, 113.7540),
    "河南省郑州市经开区经北六路": (34.7480, 113.7490),
    "河南省郑州市经开区航海东路": (34.7240, 113.7350),
    "河南省郑州市航空港区迎宾大道": (34.5260, 113.8420),
    "河南省郑州市航空港区长安路": (34.5350, 113.8280),
    "河南省郑州市郑东新区CBD": (34.7700, 113.7270),
    "河南省郑州市郑东新区东风东路": (34.7730, 113.7510),
    "河南省郑州市郑东新区金水东路": (34.7590, 113.7470),
    "河南省郑州市金水区北三环": (34.8170, 113.6810),
    "河南省郑州市金水区经三路": (34.7860, 113.7030),
    "河南省郑州市金水区花园路": (34.7950, 113.6830),
    "河南省郑州市高新区梧桐街": (34.7950, 113.5630),
    "河南省郑州市高新区瑞达路": (34.7970, 113.5880),
    "河南省郑州市高新区科学大道": (34.8100, 113.5760),
}


def display_coordinate(station_id: str, station_name: str, address: str) -> tuple[float, float]:
    """Return a stable point near the address anchor; it is not a surveyed coordinate."""
    if address not in ADDRESS_ANCHORS:
        raise ValueError(f"No display anchor configured for address: {address}")
    base_latitude, base_longitude = ADDRESS_ANCHORS[address]
    digest = hashlib.sha256(f"{station_id}|{station_name}".encode("utf-8")).digest()
    angle = int.from_bytes(digest[:4], "big") / (2**32) * math.tau
    radius = 0.0015 + int.from_bytes(digest[4:8], "big") / (2**32) * 0.0055
    latitude = base_latitude + math.sin(angle) * radius
    longitude = base_longitude + math.cos(angle) * radius / math.cos(math.radians(base_latitude))
    return round(latitude, 6), round(longitude, 6)


def district_from(address: str) -> str:
    match = re.match(r"河南省郑州市(.+?区)", address)
    return match.group(1) if match else "未知区域"


def load_rows(path: Path) -> list[tuple]:
    rows = []
    with path.open(encoding="utf-8", newline="") as source:
        for line_number, fields in enumerate(csv.reader(source, delimiter="\t"), start=1):
            if not fields or all(not item.strip() for item in fields):
                continue
            if len(fields) != 6:
                raise ValueError(f"line {line_number}: expected 6 tab-separated fields, got {len(fields)}")
            station_id, name, address, _location_id, station_type, device_count = (item.strip() for item in fields)
            if not station_id.isdigit() or not device_count.isdigit():
                raise ValueError(f"line {line_number}: station_id and device_count must be integers")
            latitude, longitude = display_coordinate(station_id, name, address)
            rows.append((int(station_id), name, address, "河南省", "郑州市", district_from(address),
                         station_type, int(device_count), latitude, longitude, "name_anchor_synthetic_v1"))
    if len({row[0] for row in rows}) != len(rows):
        raise ValueError("station_id must be unique")
    return rows


def publish(rows: list[tuple]) -> None:
    import mysql.connector

    connection = mysql.connector.connect(
        host=os.getenv("MYSQL_HOST", "127.0.0.1"), port=int(os.getenv("MYSQL_PORT", "3306")),
        user=os.environ["MYSQL_USER"], password=os.environ["MYSQL_PASSWORD"],
        database=os.getenv("MYSQL_DATABASE", "charging_ads"), autocommit=True, connection_timeout=5)
    cursor = connection.cursor()
    try:
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS station_display_coordinates (
              station_id BIGINT NOT NULL PRIMARY KEY, station_name VARCHAR(100) NOT NULL,
              address VARCHAR(200) NOT NULL, province VARCHAR(30) NOT NULL, city VARCHAR(30) NOT NULL,
              district VARCHAR(30) NOT NULL, station_type VARCHAR(30) NOT NULL,
              device_count BIGINT NOT NULL, latitude DECIMAL(10,6) NOT NULL,
              longitude DECIMAL(10,6) NOT NULL, coordinate_method VARCHAR(40) NOT NULL,
              updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        """)
        cursor.execute("CREATE TEMPORARY TABLE stage_station_display_coordinates LIKE station_display_coordinates")
        cursor.executemany(
            "INSERT INTO stage_station_display_coordinates "
            "(station_id,station_name,address,province,city,district,station_type,device_count,latitude,longitude,coordinate_method) "
            "VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)", rows)
        connection.start_transaction()
        cursor.execute("DELETE FROM station_display_coordinates")
        cursor.execute("INSERT INTO station_display_coordinates SELECT * FROM stage_station_display_coordinates")
        connection.commit()
    except Exception:
        connection.rollback()
        raise
    finally:
        cursor.close()
        connection.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Seed display-only coordinates for historical stations")
    parser.add_argument("--input", type=Path, required=True, help="Spark TSV: id, name, address, location, type, device count")
    parser.add_argument("--expected-count", type=int)
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args()
    rows = load_rows(args.input)
    if args.expected_count is not None and len(rows) != args.expected_count:
        raise SystemExit(f"expected {args.expected_count} stations, got {len(rows)}")
    if not args.validate_only:
        publish(rows)
    print(f"Validated {len(rows)} historical stations; coordinate method=name_anchor_synthetic_v1")


if __name__ == "__main__":
    main()
