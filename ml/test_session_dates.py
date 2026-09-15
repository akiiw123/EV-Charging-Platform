"""Regression coverage for explicit 2014/2015 -> 2024/2025 migration."""
import unittest
import pandas as pd
from session_dates import shift_timestamps
from train_sessions import features, TRAIN_END, TEST_START, TEST_END
from session_engine import SessionForecastEngine

class DateMigrationTests(unittest.TestCase):
    def test_legacy_and_corrected_years_map_once(self):
        dates=pd.Series(['0014-11-18 15:40:26','2015-07-01 10:30:00','2025-09-03 00:00:00'])
        actual=shift_timestamps(dates)
        self.assertEqual(actual.dt.year.tolist(),[2024,2025,2025])
        pd.testing.assert_series_equal(actual,shift_timestamps(actual))
    def test_duration_unchanged_for_dataset_year_boundary(self):
        actual=shift_timestamps(pd.Series(['2014-12-31 23:30:00','2015-01-01 01:30:00']))
        self.assertEqual(actual.iloc[1]-actual.iloc[0],pd.Timedelta(hours=2))
    def test_no_unrequested_2026_or_invalid_dates(self):
        for text in ['2026-01-01','2013-01-01','NaT','bad','2025-09-01T00:00:00Z']:
            with self.subTest(text=text),self.assertRaises(ValueError):shift_timestamps(pd.Series([text]))
    def test_weekday_features_come_from_shifted_calendar(self):
        # 2025-07-01 is Tuesday; original 2015-07-01 was Wednesday.
        orders=pd.DataFrame({'created':pd.to_datetime(['2025-06-01']), 'ended':pd.to_datetime(['2025-06-02']), 'kwhTotal':[1.]})
        x=features(orders,None,pd.Timestamp('2025-07-01'),[1])
        self.assertEqual(x[0,1],1)
    def test_splits_and_replay_shift_together(self):
        self.assertEqual(TRAIN_END,pd.Timestamp('2025-08-01'))
        self.assertEqual(TEST_START,pd.Timestamp('2025-09-01'))
        self.assertEqual(TEST_END,pd.Timestamp('2025-10-01'))
        self.assertEqual(SessionForecastEngine.parse_timestamp('2025-09-03'),pd.Timestamp('2025-09-03'))
        for value in ['2015-09-03','2026-09-03']:
            with self.assertRaises(ValueError):SessionForecastEngine.parse_timestamp(value)

if __name__=='__main__':unittest.main()

class RawDateImportTests(unittest.TestCase):
    def check_source(self, year):
        import tempfile
        from pathlib import Path
        from train_sessions import load_data
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            pd.DataFrame({'sessionId':['s1'],'stationId':['1'],
                          'created':[f'{year}-07-01 10:00:00'],'ended':[f'{year}-07-01 12:00:00'],
                          'kwhTotal':[4.],'weekday':['Wed' if year=='0015' else 'Tue'],'chargeTimeHrs':[2.]}).to_csv(root/'nvv2t.csv',index=False)
            pd.DataFrame({'stationId':['1'],'station_name':['test'],'device_count':[2]}).to_csv(root/'nvv2t_md_end.csv',index=False)
            pd.DataFrame({'record_time':['20200000000000.0']}).to_csv(root/'dsv13r2.csv',index=False)
            orders,stations,audit=load_data(root)
            self.assertEqual(orders.created.iloc[0],pd.Timestamp('2025-07-01 10:00:00'))
            self.assertEqual(orders.duration_hours.iloc[0],2.)
            self.assertEqual(audit['weekday_mismatches'],0)
            self.assertEqual(audit['date_shift']['data_kind'],'shifted_historical')
    def test_raw_legacy_year(self): self.check_source('0015')
    def test_already_shifted_raw_year(self): self.check_source('2025')
