"""Offline charging-demand experiment and read-only historical replay API."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import pandas as pd
import joblib
from sklearn.ensemble import HistGradientBoostingRegressor
from sklearn.metrics import mean_absolute_error, mean_squared_error
from session_dates import DATE_SHIFT, shift_timestamps, load_shifted_cleaned

ROOT = Path(__file__).resolve().parent
TARGETS = ['energy_kwh', 'avg_occupied', 'occupied_at_start']
TRAIN_END = pd.Timestamp('2025-08-01')
TEST_START = pd.Timestamp('2025-09-01')
TEST_END = pd.Timestamp('2025-10-01')

def write_json(path, data):
    Path(path).write_text(json.dumps(data, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')

def load_data(folder):
    folder = Path(folder)
    orders = pd.read_csv(folder/'nvv2t.csv', dtype={'stationId': str, 'sessionId': str})
    stations = pd.read_csv(folder/'nvv2t_md_end.csv', dtype={'stationId': str})
    if orders.sessionId.duplicated().any() or stations.stationId.duplicated().any():
        raise ValueError('Duplicate primary key; manual review required')
    if orders.isna().any().any() or stations.isna().any().any():
        raise ValueError('Missing source fields; manual review required')
    audit = {'orders': len(orders), 'stations': len(stations), 'date_correction': '0014/2014 -> 2024; 0015/2015 -> 2025; user-requested historical shift', 'date_shift': DATE_SHIFT,
             'assumptions': ['Complete capture inside each station first-to-last recorded session (unverified)',
                             'Uniform charging power within each session', 'Static device_count used as historical capacity (unverified)',
                             'Timezone unspecified; all timestamps interpreted on the same naive local clock'],
             'source_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.glob('*.csv')}}
    for col in ['created', 'ended']:
        orders[col+'_raw'] = orders[col]
        orders[col] = shift_timestamps(orders[col])
    original_duration = pd.to_numeric(orders.chargeTimeHrs, errors='raise')
    audit['source_weekday_values_replaced'] = int((orders.weekday != orders.created.dt.day_name().str[:3]).sum())
    orders['weekday'] = orders.created.dt.day_name().str[:3]
    duration = (orders.ended-orders.created).dt.total_seconds()/3600
    if ((duration-original_duration).abs() > 1/60).any():
        raise ValueError('Year shift changes source duration; manual review required')
    if (duration <= 0).any() or (orders.kwhTotal < 0).any():
        raise ValueError('Nonpositive duration or negative energy')
    if not orders.stationId.isin(stations.stationId).all():
        raise ValueError('Unmapped station')
    if (stations.device_count <= 0).any():
        raise ValueError('Invalid capacity')
    audit.update(date_start=str(orders.created.min()), date_end=str(orders.ended.max()),
                 zero_energy_orders=int((orders.kwhTotal == 0).sum()),
                 long_orders_over_24h=int((duration > 24).sum()),
                 weekday_mismatches=int((orders.created.dt.day_name().str[:3] != orders.weekday).sum()),
                 duration_mismatches=int(((duration-orders.chargeTimeHrs).abs() > 1/60).sum()))
    battery = pd.read_csv(folder/'dsv13r2.csv')
    audit['battery'] = {'rows': len(battery), 'unique_record_times': int(battery.record_time.nunique()), 'used_for_model': False}
    orders['duration_hours'] = duration
    orders = orders[['stationId', 'created', 'ended', 'kwhTotal', 'duration_hours']].copy()
    stations = stations[['stationId', 'station_name', 'device_count']].copy()
    return orders.sort_values('created').reset_index(drop=True), stations, audit

def aggregate(orders, stations):
    """Full bins only are evaluable; partial boundary bins retained for conservation."""
    pieces = []
    for sid, group in orders.groupby('stationId'):
        start, end = group.created.min().floor('h'), group.ended.max().ceil('h')
        times = pd.date_range(start, end, freq='h', inclusive='left')
        frame = pd.DataFrame({'hour': times, 'station_id': sid})
        energy = np.zeros(len(times)); occupied = energy.copy(); at_start = energy.copy()
        available = (times+pd.Timedelta(hours=1)).to_numpy().copy()
        for r in group.itertuples():
            left = max(0, int((r.created.floor('h')-start).total_seconds()/3600))
            right = min(len(times), int((r.ended.ceil('h')-start).total_seconds()/3600))
            for i in range(left, right):
                overlap = (min(r.ended, times[i]+pd.Timedelta(hours=1))-max(r.created, times[i])).total_seconds()/3600
                energy[i] += r.kwhTotal/r.duration_hours * overlap
                occupied[i] += overlap
                available[i] = max(available[i], r.ended.to_datetime64())
            at_start += ((times >= r.created) & (times < r.ended)).astype(float)
        frame['energy_kwh'] = energy
        frame['avg_occupied'] = occupied
        frame['occupied_at_start'] = at_start
        frame['label_available_at'] = available
        frame['evaluable'] = (times >= group.created.min()) & (times+pd.Timedelta(hours=1) <= group.ended.max())
        pieces.append(frame)
    panel = pd.concat(pieces, ignore_index=True)
    if not np.isclose(panel.energy_kwh.sum(), orders.kwhTotal.sum(), atol=1e-7):
        raise AssertionError('Energy conservation failed')
    return panel

def features(group, station, origin, horizons):
    """Future ended/kWh cannot affect features until completion has happened."""
    past = group[group.created <= origin]
    completed = past[past.ended <= origin]
    active = past[past.ended > origin]
    stats = []
    for hours in [1, 6, 24, 168]:
        cutoff = origin-pd.Timedelta(hours=hours)
        stats.append(float((past.created > cutoff).sum()))
        stats.append(float(completed.loc[completed.ended > cutoff, 'kwhTotal'].sum()))
    stats.extend([float(len(active)), float(((origin-active.created).dt.total_seconds()/3600).sum()),
                  float((origin-group.created.min()).total_seconds()/86400)])
    rows = []
    for h in horizons:
        target = origin+pd.Timedelta(hours=int(h)-1)
        # capacity is deliberately not a model input: its historical validity is unknown.
        rows.append([target.hour, target.dayofweek, int(target.dayofweek >= 5), int(h), *stats])
    return np.asarray(rows, dtype=float)

def make_samples(orders, stations, panel, begin, end):
    xs=[]; ys=[]; meta=[]
    indexed=panel.set_index(['station_id','hour'])
    station_map=stations.set_index('stationId')
    for sid, group in orders.groupby('stationId'):
        for origin in pd.date_range(begin, end, freq='24h', inclusive='left'):
            if origin < group.created.min()+pd.Timedelta(days=7):
                continue
            horizons = np.arange(1,25)
            times = [origin+pd.Timedelta(hours=int(h)-1) for h in horizons]
            # All 24 labels must lie within the split and observed station interval.
            if times[-1]+pd.Timedelta(hours=1)>pd.Timestamp(end): continue
            keys=pd.MultiIndex.from_tuples([(sid,t) for t in times],names=['station_id','hour'])
            labels=indexed.reindex(keys)
            if labels[TARGETS].isna().any().any() or not labels.evaluable.fillna(False).all(): continue
            if (labels.label_available_at > pd.Timestamp(end)).any(): continue
            xs.append(features(group,station_map.loc[sid],origin,horizons))
            ys.append(labels[TARGETS].to_numpy())
            meta.extend([{'station_id':sid,'origin':origin,'target_time':t,'horizon':int(h),'capacity':int(station_map.loc[sid,'device_count'])} for h,t in zip(horizons,times)])
    if not xs: raise ValueError('No samples for requested period')
    return np.vstack(xs),np.vstack(ys),pd.DataFrame(meta)

def fit_baseline(panel):
    p=panel[panel.evaluable & (panel.hour < TRAIN_END) & (panel.label_available_at <= TRAIN_END)].copy()
    p['dow']=p.hour.dt.dayofweek;p['hod']=p.hour.dt.hour
    return {'station': p.groupby(['station_id','dow','hod'])[TARGETS].mean(),
            'global':p.groupby(['dow','hod'])[TARGETS].mean(), 'mean':p[TARGETS].mean().to_numpy()}

def baseline_predict(base, meta):
    out=[]
    for row in meta.itertuples():
        key=(row.station_id,row.target_time.dayofweek,row.target_time.hour)
        if key in base['station'].index: v=base['station'].loc[key].to_numpy()
        elif key[1:] in base['global'].index: v=base['global'].loc[key[1:]].to_numpy()
        else:v=base['mean']
        out.append(v)
    return np.asarray(out)

def constrain(pred, capacities):
    pred=np.maximum(np.asarray(pred).copy(),0)
    pred[:,1:]=np.minimum(pred[:,1:],np.asarray(capacities)[:,None])
    return pred

def evaluate(truth, pred, meta, split, name):
    out=[]
    for horizon in [0,1,6,24]:
        mask=np.ones(len(meta),dtype=bool) if horizon==0 else meta.horizon.to_numpy()==horizon
        for j,target in enumerate(TARGETS):
            y,p=truth[mask,j],pred[mask,j]
            nz=y>0
            out.append({'split':split,'model':name,'horizon':'all' if horizon==0 else horizon,'target':target,
                        'n':len(y),'mae':float(mean_absolute_error(y,p)), 'rmse':float(np.sqrt(mean_squared_error(y,p))),
                        'nonzero_n':int(nz.sum()),'nonzero_mae':float(mean_absolute_error(y[nz],p[nz])) if nz.any() else None})
    return out

def train(folder, out, *, cleaned=False):
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    orders,stations,audit=(load_shifted_cleaned(folder) if cleaned else load_data(folder))
    panel=aggregate(orders,stations)
    panel.to_csv(out/'hourly_panel.csv.gz',index=False,compression='gzip')
    orders.to_csv(out/'anonymized_sessions.csv',index=False)
    stations.to_csv(out/'stations.csv',index=False)
    base=fit_baseline(panel)
    splits={};metrics=[]; predictions={}
    for name,begin,end in [('train','2025-03-01','2025-08-01'),('validation','2025-08-01','2025-09-01'),('test','2025-09-01','2025-10-01')]:
        print('Building',name,flush=True)
        splits[name]=make_samples(orders,stations,panel,begin,end)
    x,y,m=splits['train']
    rng=np.random.default_rng(42); selected=rng.choice(len(x),min(40000,len(x)),replace=False)
    models=[]
    for j,target in enumerate(TARGETS):
        print('Training',target,flush=True)
        model=HistGradientBoostingRegressor(max_iter=70,max_leaf_nodes=15,min_samples_leaf=40,l2_regularization=10,early_stopping=False,random_state=42)
        model.fit(x[selected],y[selected,j]);models.append(model)
    for split in ['validation','test']:
        x,y,m=splits[split]
        preds={'zero':np.zeros_like(y),'seasonal':constrain(baseline_predict(base,m),m.capacity),
               'ml':constrain(np.column_stack([model.predict(x) for model in models]),m.capacity)}
        predictions[split]=preds
        for name,p in preds.items():metrics.extend(evaluate(y,p,m,split,name))
    # Select independently per target, exclusively on validation MAE.
    chosen=[]
    for j,target in enumerate(TARGETS):
        chosen.append(min(['seasonal','ml'],key=lambda n:mean_absolute_error(splits['validation'][1][:,j],predictions['validation'][n][:,j])))
    for split in ['validation','test']:
        x,y,m=splits[split]
        p=np.column_stack([predictions[split][chosen[j]][:,j] for j in range(3)])
        metrics.extend(evaluate(y,p,m,split,'selected'))
        if split=='test':
            results=m.copy()
            for j,t in enumerate(TARGETS):results[t+'_actual']=y[:,j];results[t+'_predicted']=p[:,j]
            results.to_csv(out/'test_predictions.csv.gz',index=False,compression='gzip')
            per=[]
            for sid,idx in m.groupby('station_id').groups.items():
                for j,t in enumerate(TARGETS):per.append({'station_id':sid,'target':t,'mae':float(np.mean(np.abs(y[idx,j]-p[idx,j]))),'n':len(idx)})
            pd.DataFrame(per).to_csv(out/'station_metrics.csv',index=False)
    audit.update(sample_rows={n:len(v[0]) for n,v in splits.items()}, fitted_rows=len(selected),
                 train_label_end='2025-08-01 exclusive',model_selection_period='2025-08',test_period='2025-09',
                 evaluation_origins='daily 00:00; all horizons 1..24',selected_models=dict(zip(TARGETS,chosen)),
                 energy_conservation_error=float(abs(panel.energy_kwh.sum()-orders.kwhTotal.sum())))
    write_json(out/'data_audit.json',audit);write_json(out/'metrics.json',metrics)
    pd.DataFrame(metrics).to_csv(out/'metrics.csv',index=False)
    bundle={'models':models,'baseline':base,'chosen':chosen,'orders':orders,'stations':stations,
            'version':'charging-sessions-v2-shifted-2024-2025','date_shift':audit.get('date_shift'),'trained_before':TRAIN_END,'evaluation_end':TEST_END}
    joblib.dump(bundle,out/'model.joblib')
    lines=['# 实际回测结果','', '本数据年份已按要求由2014/2015调整为2024/2025，并非这些年份或2026年实采数据。', '本报告由训练脚本生成；小时电量为均匀分摊重建值，非电表实测。',
           '容量及采集完整性尚未经确认。每日 00:00 发起预测；结果不能代表全天任意时刻的效果。','',
           '验证集选型：'+json.dumps(audit['selected_models'],ensure_ascii=False),'',
           '| 模型 | 目标 | 测试 MAE | 非零时段 MAE |','|---|---|---:|---:|']
    for r in metrics:
        if r['split']=='test' and r['horizon']=='all':lines.append(f"| {r['model']} | {r['target']} | {r['mae']:.4f} | {r['nonzero_mae']:.4f} |")
    lines.extend(['','MAE 越小越好。zero 是稀疏数据诊断基准；selected 在 seasonal 与 ml 中按验证集选择。',
                  '若零预测总体误差更低，说明当前数据不足以证明需求预测的总体收益；应同时检查非零时段与站点指标。'])
    (out/'REPORT.md').write_text('\n'.join(lines),encoding='utf-8')
    print(json.dumps(audit,ensure_ascii=False,indent=2),flush=True)

def forecast(bundle,sid,origin,horizon=24):
    origin=pd.Timestamp(origin)
    if origin.tzinfo is not None or origin != origin.floor('h'):raise ValueError('origin must be a timezone-naive whole hour')
    if not 1<=horizon<=24:raise ValueError('horizon_hours must be between 1 and 24')
    if origin < TEST_START or origin+pd.Timedelta(hours=horizon)>TEST_END:
        raise ValueError('Replay origin must lie within September 2025, with full horizon before October 1')
    stations=bundle['stations'].set_index('stationId')
    if sid not in stations.index:raise KeyError('Unknown station_id')
    station=stations.loc[sid];group=bundle['orders'][bundle['orders'].stationId==sid]
    if origin < group.created.min()+pd.Timedelta(days=7) or origin+pd.Timedelta(hours=horizon)>group.ended.max():
        raise ValueError('Requested horizon outside this station observation interval')
    hs=np.arange(1,horizon+1);x=features(group,station,origin,hs)
    times=[origin+pd.Timedelta(hours=int(h)-1) for h in hs]
    meta=pd.DataFrame({'station_id':sid,'target_time':times})
    base=baseline_predict(bundle['baseline'],meta)
    chosen=bundle['chosen'];warnings=[]
    try:
        ml=np.column_stack([model.predict(x) for model in bundle['models']])
        pred=np.column_stack([ml[:,j] if chosen[j]=='ml' else base[:,j] for j in range(3)])
    except Exception:
        pred=base;chosen=['seasonal']*3;warnings.append('Model inference failed; seasonal fallback used')
    pred=constrain(pred,np.repeat(station.device_count,horizon))
    rows=[]
    for i,t in enumerate(times):
        rows.append({'interval_start':str(t),'interval_end':str(t+pd.Timedelta(hours=1)),
                     'energy_kwh':round(float(pred[i,0]),4),'avg_load_kw':round(float(pred[i,0]),4),
                     'avg_occupied':round(float(pred[i,1]),4),
                     'expected_idle_at_start':round(float(station.device_count-pred[i,2]),4)})
    return {'station_id':sid,'forecast_origin':str(origin),'data_cutoff':str(origin),'mode':'historical_replay',
            'model_version':bundle['version'],'selected_models':dict(zip(TARGETS,chosen)),
            'capacity':int(station.device_count),'capacity_verified':False,'load_basis':'uniform_session_energy_allocation',
            'warnings':warnings,'peak_interval':rows[int(np.argmax(pred[:,0]))]['interval_start'],'predictions':rows}


def write_platform_examples(artifacts):
    from session_engine import SessionForecastEngine
    engine = SessionForecastEngine(Path(artifacts))
    station = engine.stations()['stations'][0]
    payload = {'station_id': station['station_id'], 'horizons': [1, 6, 24]}
    write_json(Path(artifacts)/'example_forecast.json', engine.predict(payload))
    write_json(Path(artifacts)/'example_request.json', payload)
    (Path(artifacts)/'example_request.txt').write_text(
        "curl -X POST http://127.0.0.1:8090/predict -H 'Content-Type: application/json' "
        "--data '"+json.dumps(payload)+"'\n", encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description="Train sparse session forecasts for the existing platform API")
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument("--data-dir", type=Path, help="Original source CSV folder; dates are explicitly shifted")
    inputs.add_argument("--cleaned-data", type=Path, help="Supplied anonymized_sessions.csv, stations.csv and optional audit")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "artifacts" / "sessions")
    args = parser.parse_args()
    train(args.cleaned_data or args.data_dir, args.artifacts, cleaned=args.cleaned_data is not None)
    write_platform_examples(args.artifacts)


if __name__ == "__main__":
    main()
