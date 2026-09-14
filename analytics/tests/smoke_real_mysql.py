"""Run all analytics endpoints against the configured real MySQL database."""

from __future__ import annotations

from analytics_api import create_app


ENDPOINTS = (
    "/api/v1/health",
    "/api/v1/dashboard",
    "/api/v1/overview",
    "/api/v1/users/levels",
    "/api/v1/users/radar",
    "/api/v1/platforms/distribution",
    "/api/v1/charging/hourly",
    "/api/v1/stations/types",
    "/api/v1/charging/week-compare",
    "/api/v1/battery/health",
    "/api/v1/areas/costs",
    "/api/v1/stations/top?limit=10",
)


def main() -> None:
    client = create_app({"TESTING": True}).test_client()
    for endpoint in ENDPOINTS:
        response = client.get(endpoint, headers={"X-Request-ID": "real-mysql-smoke"})
        payload = response.get_json()
        if response.status_code != 200 or payload.get("code") != 0:
            raise RuntimeError(
                f"{endpoint} failed: status={response.status_code} payload={payload}"
            )
        data = payload.get("data")
        size = len(data) if isinstance(data, (dict, list)) else 0
        print(f"PASS {endpoint} size={size}")


if __name__ == "__main__":
    main()

