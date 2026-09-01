#!/usr/bin/env python3
"""Read-only HTTP API and static server for the operations dashboard."""

import argparse
import json
import sqlite3
from datetime import date, timedelta
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse


def query_dashboard(database_path: Path) -> dict:
    with sqlite3.connect(database_path, timeout=5) as connection:
        connection.row_factory = sqlite3.Row
        revenue = connection.execute(
            """SELECT
            COALESCE(SUM(CASE WHEN date(created_at,'localtime')=date('now','localtime') THEN amount END),0) today,
            COALESCE(SUM(CASE WHEN strftime('%Y-%m',created_at,'localtime')=strftime('%Y-%m','now','localtime') THEN amount END),0) month,
            COALESCE(SUM(amount),0) total
            FROM charging_orders WHERE status='completed'"""
        ).fetchone()
        statuses = {"idle": 0, "charging": 0, "fault": 0, "offline": 0}
        for row in connection.execute("SELECT status,COUNT(*) count FROM charging_piles GROUP BY status"):
            statuses[row["status"]] = row["count"]
        trend_rows = connection.execute(
            """SELECT date(created_at,'localtime') date,ROUND(SUM(amount),2) amount
            FROM charging_orders WHERE status='completed'
            AND date(created_at,'localtime')>=date('now','localtime','-6 days')
            GROUP BY date(created_at,'localtime') ORDER BY date(created_at,'localtime')"""
        )
        amounts = {row["date"]: row["amount"] for row in trend_rows}
        trend = []
        for days_ago in range(6, -1, -1):
            day = (date.today() - timedelta(days=days_ago)).isoformat()
            trend.append({"date": day, "amount": amounts.get(day, 0)})
        active_orders = connection.execute(
            "SELECT COUNT(*) FROM charging_orders WHERE status IN ('reserved','charging','awaiting_payment')"
        ).fetchone()[0]
        return {
            "metrics": {"today_revenue": revenue["today"], "month_revenue": revenue["month"],
                        "total_revenue": revenue["total"], "online_piles": statuses["idle"] + statuses["charging"],
                        "active_orders": active_orders},
            "pile_status": statuses,
            "revenue_trend": trend,
        }


class DashboardHandler(SimpleHTTPRequestHandler):
    database_path: Path

    def send_json(self, payload: dict, status: int = 200) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        if urlparse(self.path).path == "/api/dashboard":
            try:
                self.send_json(query_dashboard(self.database_path))
            except (sqlite3.Error, OSError) as error:
                self.send_json({"error": str(error)}, 500)
            return
        super().do_GET()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--database", type=Path, default=Path("../../charging_platform.db"))
    args = parser.parse_args()
    DashboardHandler.database_path = args.database.resolve()
    directory = str(Path(__file__).resolve().parent)
    handler = lambda *handler_args, **kwargs: DashboardHandler(*handler_args, directory=directory, **kwargs)
    server = ThreadingHTTPServer((args.host, args.port), handler)
    print(f"Dashboard: http://{args.host}:{args.port}  database={DashboardHandler.database_path}")
    server.serve_forever()


if __name__ == "__main__":
    main()
