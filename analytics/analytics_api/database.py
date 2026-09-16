"""封装 Flask 对 MySQL ADS 结果库的只读访问。

输入：数据库连接配置和参数化 SQL；不接收写业务数据的命令。
输出/接口：``ping``、``fetch_one``、``fetch_all``，以及十表一致读取的 ``snapshot``。
"""

from __future__ import annotations

from threading import Lock
from contextlib import contextmanager
from typing import Any, Iterable


class MySQLRepository:
    def __init__(self, config: dict):
        self._settings = {
            "host": config["MYSQL_HOST"],
            "port": config["MYSQL_PORT"],
            "user": config["MYSQL_USER"],
            "password": config["MYSQL_PASSWORD"],
            "database": config["MYSQL_DATABASE"],
            "pool_name": "charging_analytics_pool",
            "pool_size": config["MYSQL_POOL_SIZE"],
            "autocommit": True,
            "connection_timeout": 5,
        }
        self._pool_instance = None
        self._pool_lock = Lock()

    def _pool(self):
        if self._pool_instance is None:
            with self._pool_lock:
                if self._pool_instance is None:
                    from mysql.connector.pooling import MySQLConnectionPool

                    self._pool_instance = MySQLConnectionPool(**self._settings)
        return self._pool_instance

    def ping(self) -> None:
        connection = self._pool().get_connection()
        try:
            connection.ping(reconnect=False, attempts=1, delay=0)
        finally:
            connection.close()

    def fetch_one(self, query: str, params: Iterable[Any] = ()) -> dict | None:
        rows = self._execute(query, params)
        return rows[0] if rows else None

    def fetch_all(self, query: str, params: Iterable[Any] = ()) -> list[dict]:
        return self._execute(query, params)

    @contextmanager
    def snapshot(self):
        """Read all dashboard tables/metadata from the same committed batch."""
        connection = self._pool().get_connection()
        try:
            connection.start_transaction(isolation_level="REPEATABLE READ", readonly=True, consistent_snapshot=True)
            yield SnapshotRepository(connection)
        finally:
            connection.rollback()
            connection.close()

    def _execute(self, query: str, params: Iterable[Any]) -> list[dict]:
        connection = self._pool().get_connection()
        cursor = connection.cursor(dictionary=True)
        try:
            cursor.execute(query, tuple(params))
            return list(cursor.fetchall())
        finally:
            cursor.close()
            connection.close()


class SnapshotRepository:
    def __init__(self, connection):
        self.connection = connection

    def fetch_all(self, query, params=()):
        cursor = self.connection.cursor(dictionary=True)
        try:
            cursor.execute(query, tuple(params))
            return list(cursor.fetchall())
        finally:
            cursor.close()

    def fetch_one(self, query, params=()):
        rows = self.fetch_all(query, params)
        return rows[0] if rows else None
