"""Live business metrics are separate from Spark batch results."""
import os
import sqlite3
from flask import Blueprint, current_app
from .live_orders import snapshot
from .live_dashboard import dashboard_snapshot, station_snapshot
from .responses import success, failure

live_api = Blueprint('live_orders', __name__)


@live_api.get('/live/dashboard')
def dashboard():
    try:
        data, metadata = dashboard_snapshot(
            current_app.config.get('BUSINESS_DB_PATH') or os.environ.get('BUSINESS_DB_PATH'),
            current_app.config.get('LIVE_COST_PER_KWH') or os.environ.get('LIVE_COST_PER_KWH'))
        response, status = success(data, metadata=metadata)
    except (OSError, ValueError, sqlite3.Error, ArithmeticError, TypeError):
        current_app.logger.warning('Live dashboard unavailable', exc_info=True)
        response, status = failure(50302, '实时统计不可用：请检查业务数据库路径、结构与数据', 503)
    response.headers['Cache-Control'] = 'no-store'
    return response, status


@live_api.get('/live/orders')
def orders():
    try:
        data = snapshot(current_app.config.get('BUSINESS_DB_PATH') or os.environ.get('BUSINESS_DB_PATH'))
        response, status = success(data)
    except (OSError, ValueError, sqlite3.Error):
        current_app.logger.warning('Live order snapshot unavailable', exc_info=True)
        response, status = failure(50302, '实时订单不可用：请检查业务数据库路径、权限和表结构', 503)
    response.headers['Cache-Control'] = 'no-store'
    return response, status


@live_api.get('/live/stations')
def stations():
    try:
        data = station_snapshot(
            current_app.config.get('BUSINESS_DB_PATH') or os.environ.get('BUSINESS_DB_PATH'))
        response, status = success(data)
    except (OSError, ValueError, sqlite3.Error, TypeError):
        current_app.logger.warning('Live station snapshot unavailable', exc_info=True)
        response, status = failure(50302, '业务站点不可用：请检查业务数据库路径、结构与坐标', 503)
    response.headers['Cache-Control'] = 'no-store'
    return response, status
