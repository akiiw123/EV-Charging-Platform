"""Adapt session forecasts to the platform's existing /stations and /predict API."""
from datetime import datetime
from pathlib import Path
import threading

import joblib
import numpy as np
import pandas as pd

from train_sessions import forecast, TARGETS, TEST_START, TEST_END


class SessionForecastEngine:
    def __init__(self, artifacts_dir: Path, replay_at=None):
        self.bundle = joblib.load(Path(artifacts_dir) / 'model.joblib')
        self._lock = threading.Lock()
        self.horizon = 24
        self.quantiles = np.array([])  # Point models do not provide calibrated intervals.
        self.station_info = self.bundle['stations'].set_index('stationId')
        self.observations = self.bundle['orders'].groupby('stationId').agg(
            first=('created', 'min'), last=('ended', 'max'))
        self.pile_totals = self.station_info.device_count.astype(int).to_dict()
        self.zone_index = {zone: i for i, zone in enumerate(self.station_info.index)}
        if replay_at is None:
            candidates = pd.date_range(TEST_START, TEST_END-pd.Timedelta(days=1), freq='D')
            # One common replay clock; never sum unrelated dates across stations.
            self.replay_at = max(candidates, key=lambda ts: (len(self.eligible(ts)), ts))
        else:
            self.replay_at = self.parse_timestamp(replay_at)
        if not self.eligible(self.replay_at):
            raise ValueError('所选回放时间没有具备 7 天历史及完整 24 小时观察区间的站点')

    @staticmethod
    def parse_timestamp(value):
        if not isinstance(value, str):
            raise ValueError('timestamp 必须是 ISO 日期时间字符串')
        try:
            ts = pd.Timestamp(value)
        except (ValueError, TypeError):
            raise ValueError('timestamp 无法解析') from None
        if pd.isna(ts) or ts.tzinfo is not None or ts != ts.floor('h'):
            raise ValueError('timestamp 必须是不含时区的整点时间')
        if not TEST_START <= ts < TEST_END:
            raise ValueError('订单数据后端只支持 2025 年 9 月历史回放')
        return ts

    def eligible(self, timestamp):
        data = self.observations
        mask = (data['first']+pd.Timedelta(days=7) <= timestamp) & (data['last'] >= timestamp+pd.Timedelta(hours=24))
        return sorted(data.index[mask].tolist())

    def health(self):
        return {'status': 'ok', 'model': self.bundle['version'], 'backend': 'sessions',
                'mode': 'historical_replay', 'date_shift': self.bundle.get('date_shift'), 'station_namespace': 'session_dataset',
                'zones': len(self.eligible(self.replay_at)), 'horizon_hours': 24,
                'quantiles': [], 'dataset_available': True,
                'default_timestamp': self.replay_at.isoformat(),
                'data_range': [str(self.bundle['orders'].created.min()), str(self.bundle['orders'].ended.max())]}

    def stations(self):
        entries = [{'station_id': sid, 'station_name': self.station_info.loc[sid, 'station_name'],
                    'total_piles': self.pile_totals[sid]} for sid in self.eligible(self.replay_at)]
        return {'count': len(entries), 'stations': entries, 'mode': 'historical_replay', 'date_shift': self.bundle.get('date_shift'),
                'station_namespace': 'session_dataset', 'default_timestamp': self.replay_at.isoformat()}

    def predict(self, payload):
        if not isinstance(payload, dict):
            raise ValueError('请求体须为 JSON 对象')
        sid = payload.get('station_id')
        if isinstance(sid, bool) or not isinstance(sid, (str, int)):
            raise ValueError('station_id 必须是站点字符串或整数')
        sid = str(sid)
        if sid not in self.zone_index:
            raise ValueError('未知数据集站点；请查询 GET /stations，不能使用平台数据库 ID 替代')
        horizons = payload.get('horizons', [1, 6, 24])
        if not isinstance(horizons, list) or not horizons or any(type(h) is not int or not 1 <= h <= 24 for h in horizons):
            raise ValueError('horizons 须为 1~24 的非空整数数组')
        unsupported = set(payload)-{'station_id', 'timestamp', 'horizons'}
        if unsupported:
            raise ValueError('订单回放后端不接受实时历史/天气/价格覆盖字段：'+', '.join(sorted(unsupported)))
        timestamp = self.parse_timestamp(payload['timestamp']) if 'timestamp' in payload else self.replay_at
        with self._lock:
            result = forecast(self.bundle, sid, timestamp, 24)
        rows = result['predictions']
        def entry(value):
            return {'point': value, 'lower': None, 'upper': None}
        curve = [{'offset': i+1, 'timestamp': row['interval_end'].replace(' ', 'T'),
                  'interval_start': row['interval_start'].replace(' ', 'T'),
                  'interval_end': row['interval_end'].replace(' ', 'T'),
                  'load_kwh': row['energy_kwh'], 'load_lower': None, 'load_upper': None,
                  'busy_ratio': row['avg_occupied']/result['capacity']}
                 for i, row in enumerate(rows)]
        return {'station_id': sid, 'station_name': self.station_info.loc[sid, 'station_name'],
                'timestamp': timestamp.isoformat(), 'generated_at': datetime.now().isoformat(timespec='seconds'),
                'mode': 'historical_replay', 'date_shift': self.bundle.get('date_shift'), 'station_namespace': 'session_dataset',
                'model': result['model_version'], 'selected_models': result['selected_models'],
                'total_piles': result['capacity'], 'capacity_verified': False, 'horizons': horizons,
                'quantiles': [], 'interval_status': 'unavailable',
                'load_semantics': 'individual_hour_energy_kwh',
                'available_piles_semantics': 'expected_idle_at_interval_start',
                'load_basis': result['load_basis'], 'warnings': result['warnings'],
                'load_kwh': {str(h): entry(rows[h-1]['energy_kwh']) for h in horizons},
                'available_piles': {str(h): entry(rows[h-1]['expected_idle_at_start']) for h in horizons},
                'curve': curve}
