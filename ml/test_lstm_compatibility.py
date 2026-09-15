"""Small synthetic checkpoint: verifies original backend contracts, not accuracy."""
import json
from pathlib import Path
import tempfile
import unittest
import numpy as np
import pandas as pd

try:
    import torch
except ImportError:
    torch = None

from service import ForecastEngine
from data import UrbanEVData, FeatureStats


@unittest.skipIf(torch is None, 'Install ml/requirements.txt for LSTM compatibility tests')
class LstmCompatibilityTests(unittest.TestCase):
    def setUp(self):
        from model import LoadForecaster
        self.tmp = tempfile.TemporaryDirectory()
        folder = Path(self.tmp.name)
        stats = FeatureStats(0., 1., 0., 1., np.zeros(6), np.ones(6))
        model = LoadForecaster(16,14,24,1,hidden_size=4,num_layers=1,emb_dim=2,dropout=0.)
        torch.save({'state_dict':model.state_dict()},folder/'model.pt')
        meta = {'seq_len':4,'horizon':24,'quantiles':[.05,.5,.95],'stats':stats.to_dict(),
                'zones':['1'],'pile_totals':{'1':4},
                'model':{'past_dim':16,'future_dim':14,'num_zones':1,'hidden_size':4,'num_layers':1,'emb_dim':2,'dropout':0.}}
        (folder/'meta.json').write_text(json.dumps(meta))
        self.engine = ForecastEngine(folder,None)
        self.engine._data = UrbanEVData(pd.date_range('2023-01-01',periods=12,freq='h'),['1'],{'1':4},
                np.ones((12,1)),np.full((12,1),.25),np.ones((12,1)),np.zeros((12,1)),np.ones((12,6)))

    def tearDown(self):
        self.tmp.cleanup()

    def test_existing_predict_contract(self):
        result = self.engine.predict({'station_id':1})
        self.assertEqual(set(result['load_kwh']),{'1','6','24'})
        self.assertEqual(len(result['curve']),24)
        self.assertEqual(result['quantiles'],[.05,.5,.95])
        self.assertEqual(result['mode'],'historical_replay')
        json.dumps(result,allow_nan=False)

    def test_history_cannot_silently_wrap_or_reuse_stale_tail(self):
        for timestamp in ['2023-01-01T02:00','2026-09-14T00:00']:
            with self.subTest(timestamp=timestamp), self.assertRaises(ValueError):
                self.engine.predict({'station_id':1,'timestamp':timestamp})

    def test_next_hour_uses_last_complete_observation(self):
        last = self.engine.dataset.times[-1]
        self.assertEqual(self.engine._locate(last+pd.Timedelta(hours=1)),12)

    def test_explicit_history_still_supported(self):
        result = self.engine.predict({'station_id':1,'timestamp':'2026-09-14T00:00',
            'recent_load_kwh':[1,2,3,4],'recent_busy_ratio':[0,.25,.5,.25]})
        self.assertEqual(result['mode'],'provided_history')


if __name__ == '__main__':
    unittest.main()
