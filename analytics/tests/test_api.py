from __future__ import annotations

import unittest
import sqlite3
from pathlib import Path
from tempfile import TemporaryDirectory

from analytics_api import create_app


class FakeRepository:
    def __init__(self, healthy=True):
        self.healthy = healthy

    def ping(self):
        if not self.healthy:
            raise RuntimeError("database unavailable")

    def fetch_one(self, query, params=()):
        if "ads_kpi_overview" in query:
            return {
                "sessions": 3395,
                "total_kwh": 10240.5,
                "total_fee": 8610.2,
                "station_count": 105,
                "abnormal_rate": 1.2,
            }
        return None

    def fetch_all(self, query, params=()):
        if "ads_station_topn" in query:
            return [{"rn": 1, "station_name": "测试站", "station_area": "A区"}]
        if "ads_hour_trend" in query:
            return [{"hour": 8, "sessions": 12, "total_kwh": 50.0, "is_peak": 1}]
        return []


class AnalyticsApiTest(unittest.TestCase):
    def make_client(self, repository=None, overrides=None):
        config = {
            "TESTING": True,
            "ANALYTICS_CORS_ORIGINS": "http://localhost:5173",
        }
        config.update(overrides or {})
        app = create_app(
            config,
            repository or FakeRepository(),
        )
        return app.test_client()

    def test_health_uses_standard_envelope(self):
        response = self.make_client().get("/api/v1/health", headers={"X-Request-ID": "test-1"})
        self.assertEqual(response.status_code, 200)
        payload = response.get_json()
        self.assertEqual(payload["code"], 0)
        self.assertEqual(payload["request_id"], "test-1")
        self.assertEqual(payload["data"]["mysql"], "up")

    def test_health_reports_dependency_failure(self):
        response = self.make_client(FakeRepository(healthy=False)).get("/api/v1/health")
        self.assertEqual(response.status_code, 503)
        self.assertEqual(response.get_json()["code"], 50301)

    def test_overview_returns_ads_result(self):
        response = self.make_client().get("/api/v1/overview")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.get_json()["data"]["sessions"], 3395)

    def test_station_limit_validation(self):
        response = self.make_client().get("/api/v1/stations/top?limit=101")
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.get_json()["code"], 40001)

    def test_station_limit_must_be_integer(self):
        response = self.make_client().get("/api/v1/stations/top?limit=all")
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.get_json()["code"], 40001)

    def test_not_found_uses_standard_envelope(self):
        response = self.make_client().get("/api/v1/not-found")
        self.assertEqual(response.status_code, 404)
        self.assertEqual(response.get_json()["code"], 40001)

    def test_cors_is_limited_to_configured_origin(self):
        client = self.make_client()
        allowed = client.get("/api/v1/overview", headers={"Origin": "http://localhost:5173"})
        denied = client.get("/api/v1/overview", headers={"Origin": "http://example.invalid"})
        self.assertEqual(allowed.headers.get("Access-Control-Allow-Origin"), "http://localhost:5173")
        self.assertIsNone(denied.headers.get("Access-Control-Allow-Origin"))

    def test_dashboard_contains_all_result_groups(self):
        response = self.make_client().get("/api/v1/dashboard")
        self.assertEqual(response.status_code, 200)
        data = response.get_json()["data"]
        self.assertIn("overview", data)
        self.assertIn("hour_trend", data)
        self.assertIn("top_stations", data)
        self.assertEqual(len(data), 10)

    def test_dashboard_build_is_served_under_stable_path(self):
        with TemporaryDirectory() as directory:
            dist = Path(directory)
            (dist / "assets").mkdir()
            (dist / "index.html").write_text("<main>VoltFlow</main>", encoding="utf-8")
            (dist / "assets" / "app.js").write_text("window.ready = true", encoding="utf-8")
            client = self.make_client(overrides={"DASHBOARD_DIST_DIR": str(dist)})

            root = client.get("/")
            redirect_response = client.get("/dashboard")
            index = client.get("/dashboard/")
            asset = client.get("/dashboard/assets/app.js")
            history_fallback = client.get("/dashboard/settings")

            self.assertEqual(root.status_code, 302)
            self.assertTrue(root.headers["Location"].endswith("/dashboard/"))
            self.assertEqual(redirect_response.status_code, 302)
            self.assertTrue(redirect_response.headers["Location"].endswith("/dashboard/"))
            self.assertIn(b"VoltFlow", index.data)
            self.assertEqual(asset.data, b"window.ready = true")
            self.assertIn(b"VoltFlow", history_fallback.data)
            for response in (root, redirect_response, index, asset, history_fallback):
                response.close()

    def test_missing_dashboard_build_returns_standard_404(self):
        with TemporaryDirectory() as directory:
            missing = Path(directory) / "missing"
            response = self.make_client(
                overrides={"DASHBOARD_DIST_DIR": str(missing)}
            ).get("/dashboard/")

            self.assertEqual(response.status_code, 404)
            self.assertEqual(response.get_json()["code"], 40001)

    def test_live_endpoints_use_read_only_business_database(self):
        with TemporaryDirectory() as directory:
            database = Path(directory) / "business.db"
            connection = sqlite3.connect(database)
            connection.executescript("""
                CREATE TABLE users (id INTEGER PRIMARY KEY);
                CREATE TABLE charging_stations (
                    id INTEGER PRIMARY KEY, name TEXT, address TEXT, province TEXT,
                    city TEXT, district TEXT, latitude REAL, longitude REAL, status TEXT);
                CREATE TABLE charging_piles (
                    id INTEGER PRIMARY KEY, station_id INTEGER, code TEXT, type TEXT, status TEXT);
                CREATE TABLE charging_orders (
                    id INTEGER PRIMARY KEY, user_id INTEGER, pile_id INTEGER, status TEXT,
                    created_at TEXT, started_at TEXT, ended_at TEXT, energy_kwh REAL,
                    amount REAL, occupancy_fee REAL);
                INSERT INTO users VALUES (1);
                INSERT INTO charging_stations VALUES
                    (1,'测试业务站','测试地址','上海市','上海市','浦东新区',31.2,121.4,'active');
                INSERT INTO charging_piles VALUES (1,1,'P-1','fast','idle');
                INSERT INTO charging_piles VALUES (2,1,'P-2','slow','fault');
                INSERT INTO charging_orders VALUES
                    (1,1,1,'completed','2026-09-16 01:00:00','2026-09-16 01:00:00',
                     '2026-09-16 02:00:00',12.5,15.0,1.0);
            """)
            connection.commit()
            connection.close()
            client = self.make_client(overrides={"BUSINESS_DB_PATH": str(database)})

            orders = client.get("/api/v1/live/orders")
            dashboard = client.get("/api/v1/live/dashboard")
            stations = client.get("/api/v1/live/stations")

            self.assertEqual(orders.status_code, 200)
            self.assertEqual(orders.get_json()["data"]["total_orders"], 1)
            self.assertEqual(dashboard.status_code, 200)
            live_data = dashboard.get_json()["data"]
            self.assertEqual(live_data["overview"]["station_count"], 1)
            self.assertEqual(sum(row["pile_count"] for row in live_data["pile_status"]), 2)
            self.assertEqual(stations.status_code, 200)
            station_data = stations.get_json()["data"]
            self.assertEqual(station_data["source"], "platform_sqlite")
            self.assertEqual(len(station_data["stations"]), 1)
            self.assertEqual(station_data["stations"][0]["counts"]["fault"], 1)


if __name__ == "__main__":
    unittest.main()
