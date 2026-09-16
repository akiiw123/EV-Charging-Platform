"""验证 Flask 对独立预测服务的同源代理与降级响应。"""

from __future__ import annotations

import json
import unittest
from unittest.mock import patch
from urllib.error import URLError

from analytics_api import create_app

from test_api import FakeRepository


class FakeResponse:
    def __init__(self, payload, status=200):
        self.payload = payload
        self.status = status

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        return False

    def read(self):
        return json.dumps(self.payload, ensure_ascii=False).encode("utf-8")


class MlProxyTest(unittest.TestCase):
    def make_client(self):
        return create_app(
            {"TESTING": True, "ML_SERVICE_URL": "http://ml.test:8090"},
            FakeRepository(),
        ).test_client()

    @patch("analytics_api.ml_routes.urlopen")
    def test_stations_are_relayed_without_changing_namespace(self, mocked):
        mocked.return_value = FakeResponse({
            "count": 1,
            "station_namespace": "session_dataset",
            "stations": [{"station_id": "129465", "station_name": "测试站"}],
        })
        response = self.make_client().get("/api/v1/ml/stations")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.get_json()["station_namespace"], "session_dataset")
        self.assertTrue(mocked.call_args.args[0].full_url.endswith("/stations"))

    @patch("analytics_api.ml_routes.urlopen")
    def test_prediction_payload_is_relayed(self, mocked):
        mocked.return_value = FakeResponse({"model": "sessions-v2", "curve": []})
        response = self.make_client().post(
            "/api/v1/ml/predict",
            json={"station_id": "129465", "horizons": [1, 6, 24]},
        )
        self.assertEqual(response.status_code, 200)
        request_body = json.loads(mocked.call_args.args[0].data.decode("utf-8"))
        self.assertEqual(request_body["station_id"], "129465")

    @patch("analytics_api.ml_routes.urlopen", side_effect=URLError("offline"))
    def test_unavailable_model_is_explicit(self, _mocked):
        response = self.make_client().get("/api/v1/ml/health")
        self.assertEqual(response.status_code, 503)
        self.assertEqual(response.get_json()["error"], "模型服务不可用")

    def test_prediction_requires_json_object(self):
        response = self.make_client().post("/api/v1/ml/predict", data="[]", content_type="application/json")
        self.assertEqual(response.status_code, 400)
        self.assertIn("JSON 对象", response.get_json()["error"])


if __name__ == "__main__":
    unittest.main()
