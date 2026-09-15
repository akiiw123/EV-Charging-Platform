"""Read-only endpoints backed by the ten ADS result tables."""

from __future__ import annotations

from flask import Blueprint, current_app, request
from contextlib import nullcontext

from .responses import failure, success
from .metrics import metadata

api = Blueprint("analytics_api", __name__)

QUERIES = {
    "user_levels": "SELECT user_level, user_count FROM ads_user_level_dist ORDER BY user_count DESC, user_level",
    "user_radar": "SELECT user_level, dim_name, dim_value FROM ads_user_radar ORDER BY user_level, dim_name",
    "platforms": "SELECT phone_type, user_count FROM ads_platform_dist ORDER BY user_count DESC, phone_type",
    "hour_trend": "SELECT hour, sessions, total_kwh, is_peak FROM ads_hour_trend ORDER BY hour",
    "station_types": "SELECT gun_type, utilization_rate, daily_kwh, avg_fee_per_kwh FROM ads_station_type_eff ORDER BY gun_type",
    "week_compare": "SELECT day_type, sessions, total_kwh, pct FROM ads_week_compare ORDER BY day_type",
    "battery_health": "SELECT health_level, sess_count, ratio FROM ads_battery_health ORDER BY health_level",
    "area_costs": "SELECT station_area, revenue, cost, profit, profit_rate FROM ads_area_cost ORDER BY revenue DESC, station_area",
}


def _repository():
    return current_app.extensions["analytics_repository"]


@api.get("/health")
def health():
    try:
        _repository().ping()
    except Exception:
        current_app.logger.exception("MySQL health check failed")
        return failure(50301, "MySQL 结果库不可用", 503, {"mysql": "down"})
    return success({"service": "charging-analytics", "mysql": "up"})


@api.get("/overview")
def overview():
    row = _repository().fetch_one(
        "SELECT sessions, total_kwh, total_fee, station_count, abnormal_rate FROM ads_kpi_overview LIMIT 1"
    )
    return success(row or {})


def _list_endpoint(key: str):
    return success(_repository().fetch_all(QUERIES[key]))


@api.get("/users/levels")
def user_levels():
    return _list_endpoint("user_levels")


@api.get("/users/radar")
def user_radar():
    return _list_endpoint("user_radar")


@api.get("/platforms/distribution")
def platform_distribution():
    return _list_endpoint("platforms")


@api.get("/charging/hourly")
def charging_hourly():
    return _list_endpoint("hour_trend")


@api.get("/stations/types")
def station_types():
    return _list_endpoint("station_types")


@api.get("/charging/week-compare")
def week_compare():
    return _list_endpoint("week_compare")


@api.get("/battery/health")
def battery_health():
    return _list_endpoint("battery_health")


@api.get("/areas/costs")
def area_costs():
    return _list_endpoint("area_costs")


@api.get("/stations/top")
def station_top():
    raw_limit = request.args.get("limit", "10")
    try:
        limit = int(raw_limit)
    except ValueError:
        return failure(40001, "limit 必须是整数", 400)
    if not 1 <= limit <= 100:
        return failure(40001, "limit 必须在 1 到 100 之间", 400)
    rows = _repository().fetch_all(
        "SELECT rn, station_name, station_area, total_sessions, total_kwh, total_fee, utilization_rate "
        "FROM ads_station_topn ORDER BY rn LIMIT %s",
        (limit,),
    )
    return success(rows)


@api.get("/dashboard")
def dashboard():
    repository = _repository()
    context = repository.snapshot() if hasattr(repository, "snapshot") else nullcontext(repository)
    with context as snapshot:
        data = _dashboard_data(snapshot)
        batch_metadata = metadata(snapshot)
    return success(data, metadata=batch_metadata)


@api.get("/metadata")
def batch_metadata():
    return success(metadata(_repository()))


def _dashboard_data(repository):
    overview_row = repository.fetch_one(
        "SELECT sessions, total_kwh, total_fee, station_count, abnormal_rate FROM ads_kpi_overview LIMIT 1"
    )
    top_stations = repository.fetch_all(
        "SELECT rn, station_name, station_area, total_sessions, total_kwh, total_fee, utilization_rate "
        "FROM ads_station_topn ORDER BY rn LIMIT %s",
        (10,),
    )
    data = {"overview": overview_row or {}, "top_stations": top_stations}
    data.update({key: repository.fetch_all(query) for key, query in QUERIES.items()})
    return data
