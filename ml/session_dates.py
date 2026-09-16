"""显式迁移历史会话日期，不制造 2026 年“真实采集”数据。

输入：原始订单/站点时间列，其中 2014/2015（含 0014/0015）是旧历史年份。
输出/接口：``shift_timestamps`` 等函数映射到 2024/2025，并返回可审计的迁移说明。
"""
import json
from pathlib import Path
import pandas as pd

DATE_SHIFT = {'year_mapping': {'2014': 2024, '2015': 2025},
              'legacy_year_mapping': {'0014': 2024, '0015': 2025},
              'data_kind': 'shifted_historical',
              'notice': 'Dates shifted for demonstration; not observations collected in 2024, 2025 or 2026.'}


def shift_timestamps(values):
    text = values.astype(str).str.replace(r'^(?:0014|2014)(?=-)', '2024', regex=True)
    text = text.str.replace(r'^(?:0015|2015)(?=-)', '2025', regex=True)
    result = pd.to_datetime(text, errors='raise')
    if result.isna().any() or result.dt.tz is not None:
        raise ValueError('Dates must be non-null and timezone-naive')
    if not result.dt.year.isin([2024, 2025]).all():
        raise ValueError('Session dataset must contain only 2024/2025 after explicit year mapping')
    return result


def load_shifted_cleaned(folder):
    """Read supplied anonymized artifacts; source CSV ingestion is not implied."""
    folder = Path(folder)
    orders = pd.read_csv(folder/'anonymized_sessions.csv', dtype={'stationId': str})
    stations = pd.read_csv(folder/'stations.csv', dtype={'stationId': str})
    required = ['stationId', 'created', 'ended', 'kwhTotal', 'duration_hours']
    if set(orders.columns) != set(required):
        raise ValueError('Unexpected anonymized session schema')
    if orders.isna().any().any() or stations.isna().any().any():
        raise ValueError('Missing input values')
    if stations.stationId.duplicated().any() or not orders.stationId.isin(stations.stationId).all():
        raise ValueError('Duplicate or missing station key')
    audit = json.loads((folder/'data_audit.json').read_text()) if (folder/'data_audit.json').exists() else {}
    for col in ['created', 'ended']:
        orders[col] = shift_timestamps(orders[col])
    duration = (orders.ended-orders.created).dt.total_seconds()/3600
    if (duration <= 0).any() or ((duration-orders.duration_hours).abs()>1e-9).any():
        raise ValueError('Year shift changes an order duration; manual review required')
    if (orders.kwhTotal<0).any() or (stations.device_count<=0).any():
        raise ValueError('Invalid energy or capacity')
    orders['duration_hours'] = duration
    audit.update(date_shift=DATE_SHIFT, date_correction='0014/2014 -> 2024; 0015/2015 -> 2025; user-requested historical shift',
                 date_start=str(orders.created.min()),date_end=str(orders.ended.max()),
                 input_kind='retained_anonymized_sessions; original raw source ingestion not retested',
                 weekday_features='recomputed from shifted dates, not copied from original calendar')
    return orders.sort_values('created').reset_index(drop=True), stations[['stationId','station_name','device_count']].copy(), audit
