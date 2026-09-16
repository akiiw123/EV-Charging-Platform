"""Live aggregate charts from a single read-only SQLite snapshot (no Spark claims)."""
from collections import Counter, defaultdict
from contextlib import closing
from datetime import datetime, timezone, timedelta
from decimal import Decimal
from pathlib import Path
import sqlite3

SHANGHAI = timezone(timedelta(hours=8))
SESSION_STATES = {'charging', 'awaiting_payment', 'completed'}


def local_time(value):
    # Qt writes SQLite CURRENT_TIMESTAMP in UTC without an offset.
    parsed = datetime.fromisoformat(value.replace('Z', '+00:00'))
    return (parsed.replace(tzinfo=timezone.utc) if parsed.tzinfo is None else parsed).astimezone(SHANGHAI)


def numeric(value):
    result = Decimal(str(value))
    if not result.is_finite() or result < 0:
        raise ValueError('Invalid nonnegative metric')
    return result


def dashboard_snapshot(database_path, cost_per_kwh=None):
    if not database_path:
        raise ValueError('Missing business database')
    cost = None if cost_per_kwh in (None, '') else numeric(cost_per_kwh)
    path = Path(database_path).expanduser().resolve(strict=True)
    with closing(sqlite3.connect(path.as_uri() + '?mode=ro', uri=True, timeout=2)) as db:
        db.row_factory = sqlite3.Row
        db.execute('PRAGMA query_only=ON')
        db.execute('BEGIN')
        stations = {r['id']: dict(r) for r in db.execute('SELECT id,name,city FROM charging_stations')}
        piles = {r['id']: dict(r) for r in db.execute('SELECT id,station_id,type FROM charging_piles')}
        users = {r['id']: [0, Decimal(0), Decimal(0)] for r in db.execute('SELECT id FROM users')}
        orders = [dict(r) for r in db.execute('SELECT id,user_id,pile_id,status,created_at,started_at,ended_at,energy_kwh,amount,occupancy_fee FROM charging_orders')]
    # The lists above were read in one transaction; aggregate after releasing the read lock.
    station_stats = {sid: [0, Decimal(0), Decimal(0)] for sid in stations}
    type_stats = {r['type']: [0, Decimal(0), Decimal(0)] for r in piles.values()}
    type_piles = Counter(r['type'] for r in piles.values())
    hours = [[0, Decimal(0)] for _ in range(24)]
    week = [[0, Decimal(0)] for _ in range(2)]
    areas = defaultdict(lambda: [Decimal(0), Decimal(0)])
    sessions = 0
    energy = Decimal(0)
    revenue = Decimal(0)
    dates = []
    for order in orders:
        if order['status'] not in SESSION_STATES | {'reserved', 'cancelled'}:
            raise ValueError('Invalid order status')
        pile = piles.get(order['pile_id'])
        if not pile or pile['station_id'] not in stations or order['user_id'] not in users:
            raise ValueError('Broken order relationship')
        kwh, amount, fee = (numeric(order[k]) for k in ('energy_kwh','amount','occupancy_fee'))
        if order['status'] not in SESSION_STATES:
            continue
        moment = local_time(order['started_at'] or order['created_at'])
        if order['ended_at'] and local_time(order['ended_at']) < moment:
            raise ValueError('Invalid order interval')
        settled = amount + fee if order['status'] == 'completed' else Decimal(0)
        sessions += 1
        energy += kwh
        revenue += settled
        dates.append(moment.date())
        hours[moment.hour][0] += 1
        hours[moment.hour][1] += kwh
        day = int(moment.weekday() >= 5)
        week[day][0] += 1
        week[day][1] += kwh
        for stats in (station_stats[pile['station_id']], type_stats[pile['type']], users[order['user_id']]):
            stats[0] += 1
            stats[1] += kwh
            stats[2] += settled
        area = (stations[pile['station_id']]['city'] or '').strip() or '未知区域'
        areas[area][0] += settled
        areas[area][1] += kwh
    span = (max(dates) - min(dates)).days + 1 if dates else 1
    groups = defaultdict(list)
    for count, kwh, fee in users.values():
        level = '高频用户' if count >= 10 else '中频用户' if count >= 3 else '低频用户'
        groups[level].append([Decimal(count), kwh, fee, kwh / count if count else Decimal(0)])
    means = {level: [sum(row[i] for row in rows) / len(rows) for i in range(4)] for level, rows in groups.items()}
    radar = []
    for i, name in enumerate(('充电频次','累计电量','消费金额','单次电量')):
        if not means: break
        low, high = min(v[i] for v in means.values()), max(v[i] for v in means.values())
        for level, values in means.items():
            radar.append({'user_level': level, 'dim_name': name,
                          'dim_value': float((values[i]-low)/(high-low)*100) if high != low else None})
    max_station = max((r[0] for r in station_stats.values()), default=0)
    max_type = max((r[0] for r in type_stats.values()), default=0)
    ranked = sorted(station_stats, key=lambda sid: (-station_stats[sid][0], -station_stats[sid][1], sid))[:10]
    data = {
        'overview': {'sessions': sessions, 'total_kwh': float(energy), 'total_fee': float(revenue),
                     'station_count': len(stations), 'abnormal_rate': None},
        'user_levels': [{'user_level': level, 'user_count': len(rows)} for level, rows in sorted(groups.items())],
        'user_radar': radar, 'platforms': [], 'battery_health': [],
        'hour_trend': [{'hour': h, 'sessions': row[0], 'total_kwh': float(row[1]),
                        'is_peak': int(row[0] > 0 and row[0] >= sessions/24)} for h, row in enumerate(hours)],
        'week_compare': [{'day_type': ('工作日','周末')[i], 'sessions': row[0], 'total_kwh': float(row[1]),
                          'pct': row[0]/sessions*100 if sessions else 0} for i, row in enumerate(week)],
        'station_types': [{'gun_type': kind, 'utilization_rate': row[0]/max_type*100 if max_type else 0,
                           'daily_kwh': float(row[1]/type_piles[kind]/span),
                           'avg_fee_per_kwh': float(row[2]/row[1]) if row[1] else None}
                          for kind, row in sorted(type_stats.items())],
        'top_stations': [{'rn': rank+1, 'station_name': stations[sid]['name'],
                          'station_area': stations[sid]['city'] or '未知区域',
                          'total_sessions': station_stats[sid][0], 'total_kwh': float(station_stats[sid][1]),
                          'total_fee': float(station_stats[sid][2]),
                          'utilization_rate': station_stats[sid][0]/max_station*100 if max_station else 0}
                         for rank, sid in enumerate(ranked)],
        'area_costs': [{'station_area': area, 'revenue': float(row[0]),
                        'cost': float(row[1]*cost) if cost is not None else None,
                        'profit': float(row[0]-row[1]*cost) if cost is not None else None,
                        'profit_rate': float((row[0]-row[1]*cost)/row[0]*100) if cost is not None and row[0] else None}
                       for area, row in sorted(areas.items())],
    }
    metadata = {'mode': 'live_business', 'source': 'platform_sqlite', 'scope': 'all_orders',
                'generated_at': datetime.now(timezone.utc).isoformat(timespec='seconds'),
                'quality': {'raw_count': len(orders), 'platform_rows': 0, 'soc_rows': 0,
                            'cost_per_kwh': float(cost) if cost is not None else None},
                'definitions': {'hour_trend': '全部历史会话按上海时间开始小时分组，电量归入开始小时，非逐小时电表值',
                                'abnormal_rate': '未运行 Spark 清洗，不提供剔除比例',
                                'energy': '订单已保存电量，不估算正在充电的瞬时电量'}}
    return data, metadata
