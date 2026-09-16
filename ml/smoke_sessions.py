"""使用真实 sessions 模型产物对预测 HTTP 服务做冒烟测试。

输入：``--artifacts`` 模型目录，脚本会临时启动随机本机端口。
输出/接口：验证 /health、/stations、/predict 和错误参数响应，结束后关闭测试服务。
"""
import argparse
import json
from pathlib import Path
import threading
from http.server import ThreadingHTTPServer
from urllib.request import Request, urlopen
from urllib.error import HTTPError

from service import ForecastHandler
from session_engine import SessionForecastEngine


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--artifacts', type=Path, default=Path(__file__).resolve().parent/'artifacts/sessions')
    args = parser.parse_args()
    engine = SessionForecastEngine(args.artifacts)
    class Handler(ForecastHandler):
        def log_message(self, *args):
            pass
    Handler.engine = engine
    server = ThreadingHTTPServer(('127.0.0.1',0),Handler)
    worker = threading.Thread(target=server.serve_forever,daemon=True)
    worker.start()
    def request(path, payload=None, raw=None):
        body = raw if raw is not None else (json.dumps(payload).encode() if payload is not None else None)
        req = Request(f'http://127.0.0.1:{server.server_port}'+path, data=body,
                      headers={'Content-Type':'application/json'})
        try:
            with urlopen(req,timeout=20) as r:
                return r.status, json.load(r)
        except HTTPError as e:
            return e.code, json.load(e)
    try:
        checks=[]
        status, health=request('/health')
        assert status==200 and health['mode']=='historical_replay';checks.append('health')
        status, stations=request('/stations')
        assert status==200 and stations['count']>=6;checks.append('station listing')
        origins=set()
        for entry in stations['stations'][:6]:
            status, response=request('/predict',{'station_id':entry['station_id']})
            assert status==200 and len(response['curve'])==24
            assert set(response['load_kwh'])=={'1','6','24'}
            assert response['station_name']==entry['station_name']
            assert response['quantiles']==[]
            assert response['load_kwh']['24']['lower'] is None
            origins.add(response['timestamp'])
            for h in ['1','6','24']:
                assert 0<=response['available_piles'][h]['point']<=response['total_piles']
            json.dumps(response,allow_nan=False)
        assert len(origins)==1;checks.append('six UI requests, common origin, numeric bounds, nullable intervals')
        sid=stations['stations'][0]['station_id']
        for payload in [{}, {'station_id':'not-a-station'}, {'station_id':sid,'horizons':[25]},
                        {'station_id':sid,'timestamp':'2026-09-14'},
                        {'station_id':sid,'recent_load_kwh':[0]*24}, []]:
            assert request('/predict',payload)[0]==400
        checks.append('six invalid payloads return 400')
        assert request('/predict',raw=b'{bad json')[0]==400;checks.append('malformed JSON')
        assert request('/missing')[0]==404;checks.append('404')
        assert not {'userId','user_id','sessionId','phone','nickname','platform'} & set(engine.bundle['orders'].columns)
        checks.append('no user identifiers in trained artifact')
        result={'status':'passed','checks':checks,'eligible_stations':stations['count'],
                'default_timestamp':health['default_timestamp']}
        (args.artifacts/'http_validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
        print(json.dumps(result,ensure_ascii=False,indent=2))
    finally:
        server.shutdown();server.server_close();worker.join(timeout=5)


if __name__=='__main__':
    main()
