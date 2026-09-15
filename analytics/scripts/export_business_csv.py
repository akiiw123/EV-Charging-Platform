"""Export a read-only, consistent SQLite snapshot without phone numbers/passwords."""
from __future__ import annotations

import argparse
import csv
import sqlite3
from pathlib import Path

EXPORTS = {
    "orders/charging_orders.csv": ("charging_orders", "id,user_id,pile_id,status,started_at,ended_at,energy_kwh,amount,occupancy_fee,created_at"),
    "stations/charging_stations.csv": ("charging_stations", "id,name,address,province,city,district,created_at"),
    "piles/charging_piles.csv": ("charging_piles", "id,station_id,type,power_kw,status"),
    "users/users.csv": ("users", "id,created_at"),
}


def export_database(database: Path, output: Path):
    if not database.is_file():
        raise ValueError("SQLite database does not exist; start the Qt admin first")
    connection = sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True)
    try:
        connection.execute("BEGIN")
        for relative, (table, columns) in EXPORTS.items():
            available = {row[1] for row in connection.execute(f"PRAGMA table_info({table})")}
            expressions = []
            for column in columns.split(","):
                if column in available:
                    expressions.append(column)
                elif column == "occupancy_fee":
                    expressions.append("0 AS occupancy_fee")
                elif column in ("province", "city", "district"):
                    expressions.append(f"'' AS {column}")
                else:
                    raise ValueError(f"{table} missing column {column}")
            # Preserve optional measured fields only when they really exist.
            if table == "charging_orders":
                expressions += [name for name in ("platform", "soc") if name in available]
            target = output / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            cursor = connection.execute(f"SELECT {','.join(expressions)} FROM {table} ORDER BY id")
            with target.open("w", encoding="utf-8", newline="") as stream:
                writer = csv.writer(stream)
                writer.writerow([column[0] for column in cursor.description])
                writer.writerows(cursor)
    finally:
        connection.rollback()
        connection.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--db", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    export_database(args.db, args.out)
    print(f"Exported anonymized business CSV to {args.out}")
