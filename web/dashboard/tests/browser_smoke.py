import argparse
import os
import tempfile
import json
from pathlib import Path
from playwright.sync_api import sync_playwright

parser=argparse.ArgumentParser(description='Check live dashboard and isolated browser fixtures; never writes business data.')
parser.add_argument('url', nargs='?', default=os.getenv('DASHBOARD_URL','http://127.0.0.1:8091/dashboard/'))
parser.add_argument('--output', default=None)
args=parser.parse_args()
out=Path(args.output or tempfile.mkdtemp(prefix='ev-dashboard-browser-'))
out.mkdir(parents=True,exist_ok=True)
groups={
 'overview':dict(sessions=100,total_kwh=250.5,total_fee=210.25,station_count=12,abnormal_rate=1.5),
 'user_levels':[dict(user_level='高频',user_count=20)],
 'user_radar':[dict(user_level=level,dim_name=dim,dim_value=value+i*3) for level,value in [('高频',70),('低频',20)] for i,dim in enumerate(['频次','电量','金额','时长'])],
 'platforms':[dict(phone_type='Android',user_count=70)],
 'hour_trend':[dict(hour=h,sessions=h,total_kwh=h*3,is_peak=int(h==23)) for h in range(24)],
 'station_types':[dict(gun_type='快充',utilization_rate=62,daily_kwh=200,avg_fee_per_kwh=1.2)],
 'week_compare':[dict(day_type='工作日',sessions=60,total_kwh=120,pct=60)],
 'battery_health':[dict(health_level='20-40%',sess_count=80,ratio=80)],
 'area_costs':[dict(station_area='华东',revenue=100,cost=60,profit=40,profit_rate=40)],
 'top_stations':[dict(rn=i+1,station_name=f'测试站{i+1}',station_area='华东',total_sessions=100-i*8,total_kwh=400-i*20,total_fee=i*100,utilization_rate=100-i*8) for i in range(10)]
}
metadata=dict(batch_id='browser-test-only',generated_at='2026-09-15T00:00:00Z',analysis=dict(module='analytics/scripts/evcharging_analysis.py',script_sha256='a'*64),quality=dict(raw_count=101,valid_count=100,rejected_count=1,cost_per_kwh=0.8,platform_rows=70,soc_rows=80))
with sync_playwright() as p:
 browser=p.chromium.launch(headless=True, **({'executable_path':os.environ['PLAYWRIGHT_CHROMIUM_EXECUTABLE']} if os.getenv('PLAYWRIGHT_CHROMIUM_EXECUTABLE') else {}))
 try:
  page=browser.new_page(viewport=dict(width=1920,height=1080))
  errors=[]
  page.on('pageerror',lambda error:errors.append(str(error)))
  page.goto(args.url,wait_until='networkidle')
  page.wait_for_function("Number(document.querySelector('#map')?.dataset.renderedStations) > 0")
  live_station_count=page.evaluate("Number(document.querySelector('#map')?.dataset.renderedStations)")
  assert page.get_by_text('实时业务统计在线',exact=True).is_visible()
  live_source=page.locator('.metric-notes summary').inner_text()
  prediction_button=page.get_by_role('button',name='打开智能预测',exact=True)
  prediction_button.click()
  prediction_panel=page.get_by_role('region',name='智能预测卡片')
  assert prediction_panel.is_visible()
  assert '规则推演' in prediction_panel.inner_text()
  for side in ['left','right']:
   dock=page.locator(f'.performance-dock--{side}')
   dock.focus();page.wait_for_timeout(350)
   panel_box,dock_box=prediction_panel.bounding_box(),dock.bounding_box()
   overlaps=not (panel_box['x']+panel_box['width']<=dock_box['x'] or dock_box['x']+dock_box['width']<=panel_box['x'] or panel_box['y']+panel_box['height']<=dock_box['y'] or dock_box['y']+dock_box['height']<=panel_box['y'])
   assert not overlaps,f'prediction overlaps {side} performance dock'
  prediction_panel.press('Escape')
  assert not prediction_panel.is_visible()
  page.screenshot(path=str(out/'live-night.png'),full_page=True)
  print(f'PASS live service: data loads, {live_station_count} business stations; source status: '+live_source,flush=True)
  state={'failure':False,'missing':False}
  def handle(route):
   if state['failure']: route.fulfill(status=503,json={'code':50301,'message':'test unavailable'}); return
   meta=json.loads(json.dumps(metadata)); data=json.loads(json.dumps(groups))
   if state['missing']:
    meta['quality'].update(platform_rows=0,soc_rows=0)
    data['platforms']=[];data['battery_health']=[];data['user_radar'][0]['dim_value']=None
   route.fulfill(json={'code':0,'data':data,'metadata':meta})
  page.route('**/api/v1/dashboard',handle)
  page.get_by_label('图表数据来源').select_option('batch')
  page.wait_for_function("document.querySelector('.metric-notes summary')?.textContent.includes('已提供分析批次')")
  page.wait_for_function("Number(document.querySelector('#map')?.dataset.renderedStations) === 105")
  assert page.get_by_text('展示坐标 · 按站点名称近似生成', exact=True).is_visible()
  assert page.locator('.chart-canvas canvas').count()==6
  assert '已提供分析批次' in page.locator('.metric-notes summary').inner_text()
  page.locator('.metric-notes summary').click()
  assert '0.8 元/kWh' in page.locator('.metric-notes').inner_text()
  page.locator('.metric-notes summary').click()
  for side in ['left','right']:
   rail=page.locator(f'.chart-column--{side}')
   rail.focus();page.wait_for_timeout(350)
   assert f'{side}-open' in page.locator('.analytics-grid').get_attribute('class')
   rail.press('Escape')
   assert f'{side}-open' not in page.locator('.analytics-grid').get_attribute('class')
  page.get_by_role('button',name='结构与收益',exact=True).click()
  assert page.get_by_role('heading',name='终端平台偏好').count()==1
  page.locator('.chart-column--left').hover();page.wait_for_timeout(350)
  page.get_by_role('button',name='用户与时段',exact=True).click()
  for side in ['left','right']:
   dock=page.locator(f'.performance-dock--{side}')
   dock.focus();page.wait_for_timeout(350)
   assert dock.bounding_box()['height']>=295
  theme_button=page.locator('button[data-theme-mode]')
  assert theme_button.get_attribute('data-theme-mode')=='auto'
  theme_button.click()
  assert theme_button.get_attribute('data-theme-mode')=='day'
  assert page.evaluate('document.documentElement.dataset.theme')=='day'
  page.screenshot(path=str(out/'fixture-day.png'),full_page=True)
  page.reload(wait_until='networkidle')
  theme_button=page.locator('button[data-theme-mode]')
  assert theme_button.get_attribute('data-theme-mode')=='day'
  assert page.evaluate('document.documentElement.dataset.theme')=='day'
  page.get_by_label('图表数据来源').select_option('batch')
  page.wait_for_function("document.querySelector('.metric-notes summary')?.textContent.includes('已提供分析批次')")
  theme_button.click()
  assert theme_button.get_attribute('data-theme-mode')=='night'
  assert page.evaluate('document.documentElement.dataset.theme')=='night'
  assert page.get_by_role('button',name='全屏',exact=True).is_visible()
  page.get_by_role('button',name='2.5D',exact=True).click()
  assert page.get_by_role('button',name='2.5D',exact=True).get_attribute('aria-pressed')=='true'
  page.get_by_role('button',name='2D',exact=True).click()
  page.get_by_label('选择省份').select_option('110000')
  page.wait_for_timeout(350)
  assert page.locator('#mapScope').inner_text()=='北京市'
  page.get_by_role('button',name='放大地图').click()
  page.get_by_role('button',name='缩小地图').click()
  page.get_by_role('button',name='复位地图').click()
  page.get_by_role('button',name='返回全国',exact=True).click()
  assert page.locator('#mapScope').inner_text()=='全国'
  print('PASS fixture interactions: groups, keyboard rails/KPI/docks, themes persist, province/back, 2D/2.5D, zoom/reset',flush=True)
  sizes=[]
  for width,height in [(1920,1080),(1440,900),(1280,720),(1024,768),(390,844)]:
   page.set_viewport_size(dict(width=width,height=height));page.wait_for_timeout(400)
   assert page.evaluate('document.documentElement.scrollWidth <= innerWidth+1'),f'overflow at {width}'
   sizes.append([width,height])
   if width==390:
    cards=page.locator('.performance-dock').all()
    assert cards[1].bounding_box()['y']>=cards[0].bounding_box()['y']+cards[0].bounding_box()['height']
    right=page.locator('.chart-column--right').bounding_box()
    assert right['y']>=cards[1].bounding_box()['y']+cards[1].bounding_box()['height']
   if width==390:
    page.locator('#map').scroll_into_view_if_needed()
    page.wait_for_function("Number(document.querySelector('#map')?.dataset.renderedStations)>0")
   page.screenshot(path=str(out/f'fixture-{width}.png'),full_page=True)
  page.set_viewport_size(dict(width=1440,height=900))
  state['failure']=True
  page.get_by_label('图表数据来源').select_option('live')
  page.get_by_label('图表数据来源').select_option('batch')
  page.get_by_role('alert').wait_for()
  assert 'HTTP 503' in page.get_by_role('alert').inner_text()
  assert page.locator('.kpi-value strong').first.inner_text()=='100'
  state['failure']=False;state['missing']=True
  page.get_by_label('图表数据来源').select_option('live')
  page.get_by_label('图表数据来源').select_option('batch')
  page.wait_for_function("!document.querySelector('.error-banner')")
  page.get_by_role('button',name='结构与收益',exact=True).click()
  assert 'platform' in page.locator('.chart-column--left').inner_text()
  assert 'SOC' in page.locator('.chart-column--left').inner_text()
  page.emulate_media(reduced_motion='reduce')
  assert page.evaluate("matchMedia('(prefers-reduced-motion: reduce)').matches")
  assert not errors,errors
  print('PASS responsive 5 sizes, 503 preserves data, retry recovers, optional fields show empty reasons, no JavaScript errors',flush=True)
  (out/'browser-results.json').write_text(json.dumps(dict(viewports=sizes,page_errors=errors,live=live_source,fixture_checks='passed'),ensure_ascii=False,indent=2))
 finally: browser.close()
