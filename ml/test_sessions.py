"""会话预测训练、推理和 HTTP 参数校验的主单元测试集。

输入：临时构造的脱敏订单、站点和模型产物。
输出/接口：覆盖防数据泄漏、模型选择、1/6/24 小时语义、站点命名空间和错误响应。
运行：``python -m unittest discover -s ml -p 'test_*.py' -v``。
"""
import hashlib
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
import pandas as pd

from train_sessions import aggregate, features, fit_baseline, constrain
from session_engine import SessionForecastEngine
from export import read_database, build_sessions, aggregate as aggregate_sqlite


class SessionTests(unittest.TestCase):
    def setUp(self):
        self.orders = pd.DataFrame({'stationId': ['1'], 'created': [pd.Timestamp('2025-07-01 10:30')],
            'ended': [pd.Timestamp('2025-07-01 12:00')], 'kwhTotal': [9.], 'duration_hours': [1.5]})
        self.stations = pd.DataFrame({'stationId': ['1'], 'device_count': [2]})

    def test_hourly_energy_conservation_and_occupancy(self):
        p = aggregate(self.orders, self.stations)
        np.testing.assert_allclose(p.energy_kwh, [3, 6])
        np.testing.assert_allclose(p.avg_occupied, [.5, 1])
        np.testing.assert_allclose(p.occupied_at_start, [0, 1])

    def test_future_completion_values_do_not_enter_features(self):
        origin = pd.Timestamp('2025-07-01 11:00')
        a = features(self.orders, None, origin, [1, 6, 24])
        changed = self.orders.copy()
        changed['ended'] = pd.Timestamp('2025-08-01'); changed['kwhTotal'] = 99999.
        np.testing.assert_array_equal(a, features(changed, None, origin, [1, 6, 24]))

    def test_future_arrivals_do_not_enter_features(self):
        future = self.orders.copy()
        future['created'] = pd.Timestamp('2025-07-02'); future['ended'] = pd.Timestamp('2025-07-03')
        origin = pd.Timestamp('2025-07-01 11:00')
        np.testing.assert_array_equal(features(self.orders, None, origin, [1]),
            features(pd.concat([self.orders, future]), None, origin, [1]))

    def test_late_label_excluded_from_baseline(self):
        panel = pd.DataFrame({'station_id': ['1', '1'], 'hour': pd.to_datetime(['2025-07-01', '2025-07-08']),
            'evaluable': [True, True], 'label_available_at': pd.to_datetime(['2025-07-01', '2025-08-02']),
            'energy_kwh': [2., 999.], 'avg_occupied': [1., 99.], 'occupied_at_start': [1., 99.]})
        self.assertEqual(fit_baseline(panel)['mean'][0], 2.)

    def test_zero_energy_still_occupies(self):
        self.orders['kwhTotal'] = 0
        p = aggregate(self.orders, self.stations)
        self.assertEqual(p.energy_kwh.sum(), 0)
        self.assertEqual(p.avg_occupied.sum(), 1.5)

    def test_physical_bounds(self):
        np.testing.assert_array_equal(constrain([[-1, 4, 6]], [2]), [[0, 2, 2]])

    def test_replay_timestamp_validation(self):
        for value in ['2026-09-14', 'NaT', '2025-09-01T00:01', '2025-09-01T00:00Z', 1, None]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                SessionForecastEngine.parse_timestamp(value)

    def test_existing_sqlite_schema_export_stays_read_only(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'test.db'
            with sqlite3.connect(path) as conn:
                conn.executescript((Path(__file__).resolve().parents[1]/'database/schema.sql').read_text())
                conn.execute("INSERT INTO charging_stations(id,name,address,latitude,longitude,price_per_kwh) VALUES(1,'test','test',0,0,1)")
                conn.execute("INSERT INTO charging_piles(id,station_id,code,type,power_kw) VALUES(1,1,'test','slow',7)")
                conn.execute("INSERT INTO users(id,phone,nickname) VALUES(1,'00000000000','test')")
                conn.execute("INSERT INTO charging_orders(user_id,pile_id,status,started_at,ended_at,energy_kwh) VALUES(1,1,'completed','2025-07-01 10:30','2025-07-01 12:00',9)")
            before = hashlib.sha256(path.read_bytes()).hexdigest()
            stations, piles, orders = read_database(path)
            sessions, skipped = build_sessions(orders, {1:1}, {1}, pd.Timestamp('2025-07-02'))
            volume, busy = aggregate_sqlite(sessions, ['1'], {1:1}, pd.Timestamp('2025-07-01 10:00'), 2)
            self.assertEqual(skipped, 0)
            np.testing.assert_allclose(volume[:,0], [3,6])
            np.testing.assert_allclose(busy[:,0], [.5,1])
            self.assertNotIn('user_id', orders.columns)
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), before)


class AdapterContractTests(unittest.TestCase):
    def setUp(self):
        orders = pd.DataFrame({'stationId':['1','1'], 'created':pd.to_datetime(['2025-07-01','2025-10-01']),
                              'ended':pd.to_datetime(['2025-07-02','2025-10-02'])})
        bundle = {'stations':pd.DataFrame({'stationId':['1'],'station_name':['Dataset station'],'device_count':[4]}),
                  'orders':orders,'version':'test'}
        with patch('session_engine.joblib.load', return_value=bundle):
            self.engine = SessionForecastEngine(Path('/unused'))

    def test_station_namespace_and_shared_clock(self):
        response = self.engine.stations()
        self.assertEqual(response['station_namespace'], 'session_dataset')
        self.assertEqual(response['stations'][0]['station_name'], 'Dataset station')
        self.assertEqual(response['default_timestamp'], self.engine.health()['default_timestamp'])

    def test_api_shape_and_no_fabricated_intervals(self):
        origin = self.engine.replay_at
        rows = [{'interval_start':str(origin+pd.Timedelta(hours=i)),
                 'interval_end':str(origin+pd.Timedelta(hours=i+1)), 'energy_kwh':float(i+1),
                 'avg_occupied':.5,'expected_idle_at_start':3.5} for i in range(24)]
        fake = {'predictions':rows,'capacity':4,'model_version':'test','selected_models':{},
                'load_basis':'uniform_session_energy_allocation','warnings':[]}
        with patch('session_engine.forecast',return_value=fake):
            response = self.engine.predict({'station_id':1})
        self.assertEqual(response['load_kwh']['6']['point'],6.)  # individual hour, not sum 1..6
        self.assertIsNone(response['load_kwh']['6']['lower'])
        self.assertEqual(response['quantiles'],[])
        self.assertEqual(response['available_piles']['1']['point'],3.5)
        self.assertEqual(response['mode'],'historical_replay')
        self.assertEqual(len(response['curve']),24)
        json.dumps(response, allow_nan=False)

    def test_invalid_requests_are_explicit(self):
        for payload in [None, [], {}, {'station_id':True}, {'station_id':'missing'},
                        {'station_id':1,'horizons':[]}, {'station_id':1,'horizons':[True]},
                        {'station_id':1,'horizons':[25]}, {'station_id':1,'horizons':[1.0]},
                        {'station_id':1,'recent_load_kwh':[1,2,3]}]:
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                self.engine.predict(payload)


if __name__ == '__main__':
    unittest.main()
