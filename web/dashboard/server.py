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
        # 趋势扩到 30 天,前端按"近 7 日/近 30 日"切换截取
        trend_rows = connection.execute(
            """SELECT date(created_at,'localtime') date,ROUND(SUM(amount),2) amount
            FROM charging_orders WHERE status='completed'
            AND date(created_at,'localtime')>=date('now','localtime','-29 days')
            GROUP BY date(created_at,'localtime') ORDER BY date(created_at,'localtime')"""
        )
        amounts = {row["date"]: row["amount"] for row in trend_rows}
        trend = []
        for days_ago in range(29, -1, -1):
            day = (date.today() - timedelta(days=days_ago)).isoformat()
            trend.append({"date": day, "amount": amounts.get(day, 0)})
        # 站点营收排行(前 5)
        ranking = [
            {"name": row["name"], "orders": row["orders"], "revenue": row["revenue"]}
            for row in connection.execute(
                """SELECT s.name,COUNT(o.id) orders,ROUND(COALESCE(SUM(o.amount),0),2) revenue
                FROM charging_stations s
                LEFT JOIN charging_piles p ON p.station_id=s.id
                LEFT JOIN charging_orders o ON o.pile_id=p.id AND o.status='completed'
                GROUP BY s.id ORDER BY revenue DESC, s.id LIMIT 5"""
            )
        ]
        # 近 7 日订单 24 小时时段分布(本地时区)
        hourly = [0] * 24
        for row in connection.execute(
            """SELECT CAST(strftime('%H',created_at,'localtime') AS INTEGER) h,COUNT(*) c
            FROM charging_orders
            WHERE status IN ('completed','charging','awaiting_payment')
            AND created_at>=datetime('now','localtime','-6 days') GROUP BY h"""
        ):
            hourly[row["h"]] = row["c"]
        active_orders = connection.execute(
            "SELECT COUNT(*) FROM charging_orders WHERE status IN ('reserved','charging','awaiting_payment')"
        ).fetchone()[0]
        total_piles = sum(statuses.values())
        # 桩位利用率:充电中 / 全部电桩(离线视作不可用,不计入可用基数)
        usable = total_piles - statuses["offline"]
        utilization = round(statuses["charging"] * 100.0 / usable, 1) if usable else 0.0
        return {
            "metrics": {"today_revenue": revenue["today"], "month_revenue": revenue["month"],
                        "total_revenue": revenue["total"], "online_piles": statuses["idle"] + statuses["charging"],
                        "active_orders": active_orders, "utilization": utilization,
                        "total_piles": total_piles},
            "pile_status": statuses,
            "revenue_trend": trend,
            "station_ranking": ranking,
            "hourly_orders": hourly,
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
    # 默认路径按脚本位置解析(web/dashboard -> 项目根),从任意目录启动均可
    parser.add_argument("--database", type=Path,
                        default=Path(__file__).resolve().parent.parent.parent / "charging_platform.db")
    args = parser.parse_args()
    DashboardHandler.database_path = args.database.resolve()
    directory = str(Path(__file__).resolve().parent)
    handler = lambda *handler_args, **kwargs: DashboardHandler(*handler_args, directory=directory, **kwargs)
    server = ThreadingHTTPServer((args.host, args.port), handler)
    print(f"Dashboard: http://{args.host}:{args.port}  database={DashboardHandler.database_path}")
    server.serve_forever()


if __name__ == "__main__":
    main()
