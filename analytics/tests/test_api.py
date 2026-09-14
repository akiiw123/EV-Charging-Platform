from __future__ import annotations

import unittest

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
    def make_client(self, repository=None):
        app = create_app(
            {"TESTING": True, "ANALYTICS_CORS_ORIGINS": "http://localhost:5173"},
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


if __name__ == "__main__":
    unittest.main()
