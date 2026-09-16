"""创建第二阶段 Flask 应用，并把分析接口和 Vue 大屏装配到同一服务。

输入：环境变量配置，测试时也可传入配置覆盖项和假数据仓库。
输出/接口：注册 ``/api/v1/*``、``/dashboard/``、请求编号、CORS 和统一错误响应。
"""

from __future__ import annotations

import logging
import uuid
from pathlib import Path

from flask import Flask, abort, g, redirect, request, send_from_directory
from werkzeug.exceptions import HTTPException

from .config import load_config
from .database import MySQLRepository
from .responses import failure
from .routes import api


def create_app(config_overrides: dict | None = None, repository=None) -> Flask:
    app = Flask(__name__)
    app.config.from_mapping(load_config())
    if config_overrides:
        app.config.update(config_overrides)

    app.extensions["analytics_repository"] = repository or MySQLRepository(app.config)
    app.register_blueprint(api, url_prefix="/api/v1")
    from .live_routes import live_api
    app.register_blueprint(live_api, url_prefix="/api/v1")
    from .ml_routes import ml_api
    app.register_blueprint(ml_api, url_prefix="/api/v1")
    dashboard_dist = Path(app.config["DASHBOARD_DIST_DIR"]).resolve()

    def dashboard_file(filename: str):
        target = (dashboard_dist / filename).resolve()
        try:
            target.relative_to(dashboard_dist)
        except ValueError:
            abort(404)
        if not target.is_file():
            abort(404)
        return send_from_directory(dashboard_dist, filename)

    @app.get("/")
    def dashboard_root():
        if not (dashboard_dist / "index.html").is_file():
            abort(404, description="大屏尚未构建")
        return redirect("/dashboard/")

    @app.get("/dashboard")
    def dashboard_redirect():
        return redirect("/dashboard/")

    @app.get("/dashboard/")
    def dashboard_index():
        return dashboard_file("index.html")

    @app.get("/dashboard/<path:asset_path>")
    def dashboard_asset(asset_path: str):
        target = (dashboard_dist / asset_path).resolve()
        try:
            target.relative_to(dashboard_dist)
        except ValueError:
            abort(404)
        if target.is_file():
            return send_from_directory(dashboard_dist, asset_path)
        if "." not in Path(asset_path).name:
            return dashboard_file("index.html")
        abort(404)

    @app.before_request
    def assign_request_id() -> None:
        supplied = request.headers.get("X-Request-ID", "").strip()
        g.request_id = supplied[:128] if supplied else uuid.uuid4().hex

    @app.after_request
    def add_response_headers(response):
        response.headers["X-Request-ID"] = g.get("request_id", "")
        origin = request.headers.get("Origin")
        allowed = {
            item.strip()
            for item in app.config["ANALYTICS_CORS_ORIGINS"].split(",")
            if item.strip()
        }
        if origin and origin in allowed:
            response.headers["Access-Control-Allow-Origin"] = origin
            response.headers["Vary"] = "Origin"
            response.headers["Access-Control-Allow-Headers"] = "Content-Type, X-Request-ID"
            response.headers["Access-Control-Allow-Methods"] = "GET, OPTIONS"
        return response

    @app.errorhandler(HTTPException)
    def handle_http_error(error: HTTPException):
        code = 40001 if error.code and error.code < 500 else 50001
        return failure(code, error.description, error.code or 500)

    @app.errorhandler(Exception)
    def handle_unexpected_error(error: Exception):
        app.logger.exception("Unhandled analytics API error", exc_info=error)
        return failure(50001, "服务器内部错误", 500)

    logging.getLogger("werkzeug").setLevel(logging.INFO)
    return app
