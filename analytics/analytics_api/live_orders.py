"""Read-only snapshots of the platform database; never opens a missing DB writable."""
import sqlite3
from contextlib import closing
from datetime import datetime, timezone
from pathlib import Path


def snapshot(database_path):
    if not database_path:
        raise ValueError('尚未配置业务数据库')
    path = Path(database_path).expanduser().resolve(strict=True)
    if not path.is_file():
        raise ValueError('业务数据库不是文件')
    with closing(sqlite3.connect(path.as_uri() + '?mode=ro', uri=True, timeout=2)) as connection:
        connection.row_factory = sqlite3.Row
        connection.execute('PRAGMA query_only=ON')
        connection.execute('BEGIN')
        counts = {status: 0 for status in (
            'reserved', 'charging', 'awaiting_payment', 'completed', 'cancelled')}
        for row in connection.execute('SELECT status, COUNT(*) AS n FROM charging_orders GROUP BY status'):
            if row['status'] not in counts:
                raise ValueError('存在不支持的订单状态')
            counts[row['status']] = row['n']
        sums = connection.execute("""
            SELECT COALESCE(SUM(CASE WHEN status IN ('charging','awaiting_payment','completed')
                       THEN energy_kwh ELSE 0 END), 0) AS recorded_energy_kwh,
                   COALESCE(SUM(CASE WHEN status='completed'
                       THEN amount + occupancy_fee ELSE 0 END), 0) AS settled_revenue
            FROM charging_orders
        """).fetchone()
        recent = connection.execute("""
            SELECT o.id, o.status, s.name AS station_name, p.code AS pile_code,
                   o.created_at, o.started_at, o.ended_at, o.energy_kwh,
                   CASE WHEN o.status='completed' THEN o.amount + o.occupancy_fee ELSE NULL END AS settled_amount
            FROM charging_orders o
            JOIN charging_piles p ON p.id=o.pile_id
            JOIN charging_stations s ON s.id=p.station_id
            ORDER BY o.id DESC LIMIT 10
        """).fetchall()
        return {'source': 'platform_sqlite', 'scope': 'all_orders',
                'generated_at': datetime.now(timezone.utc).isoformat(timespec='seconds'),
                'counts': counts, 'total_orders': sum(counts.values()),
                **dict(sums), 'recent_orders': [dict(row) for row in recent],
                'energy_basis': 'stored_order_energy_not_live_meter'}
