"""Environment-backed configuration with no embedded credentials."""

from __future__ import annotations

import os
from pathlib import Path


DEFAULT_DASHBOARD_DIST = Path(__file__).resolve().parents[2] / "web" / "dashboard" / "dist"


def _positive_int(name: str, default: int) -> int:
    raw = os.getenv(name, str(default))
    try:
        value = int(raw)
    except ValueError as exc:
        raise RuntimeError(f"{name} must be an integer") from exc
    if value <= 0:
        raise RuntimeError(f"{name} must be positive")
    return value


def load_config() -> dict:
    return {
        "MYSQL_HOST": os.getenv("MYSQL_HOST", "127.0.0.1"),
        "MYSQL_PORT": _positive_int("MYSQL_PORT", 3306),
        "MYSQL_USER": os.getenv("MYSQL_USER", "charging_api"),
        "MYSQL_PASSWORD": os.getenv("MYSQL_PASSWORD", ""),
        "MYSQL_DATABASE": os.getenv("MYSQL_DATABASE", "charging_ads"),
        "MYSQL_POOL_SIZE": _positive_int("MYSQL_POOL_SIZE", 5),
        "ANALYTICS_CORS_ORIGINS": os.getenv(
            "ANALYTICS_CORS_ORIGINS", "http://localhost:5173"
        ),
        "DASHBOARD_DIST_DIR": os.getenv(
            "DASHBOARD_DIST_DIR", str(DEFAULT_DASHBOARD_DIST)
        ),
    }
