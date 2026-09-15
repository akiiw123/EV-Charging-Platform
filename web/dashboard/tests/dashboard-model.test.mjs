import test from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import { normalizeDashboard, normalizeStations, hasVerifiedSource, chartMissingReason } from '../src/lib/dashboard-model.js'
import { buildChartOptions } from '../src/lib/chart-options.js'

const groups = {
  overview: { sessions: 100, total_kwh: 250.5, total_fee: 210.25, station_count: 12, abnormal_rate: 1.5 },
  user_levels: [{ user_level: '高频', user_count: 20 }],
  user_radar: [{ user_level: '高频', dim_name: '频次', dim_value: 80 }],
  platforms: [{ phone_type: 'Android', user_count: 70 }],
  hour_trend: [{ hour: 8, sessions: 12, total_kwh: 48, is_peak: 1 }],
  station_types: [{ gun_type: '快充', utilization_rate: 62, daily_kwh: 200, avg_fee_per_kwh: 1.2 }],
  week_compare: [{ day_type: '工作日', sessions: 60, total_kwh: 120, pct: 60 }],
  battery_health: [{ health_level: '健康', sess_count: 80, ratio: 80 }],
  area_costs: [{ station_area: '华东', revenue: 100, cost: 60, profit: 40, profit_rate: 40 }],
  top_stations: [{ rn: 1, station_name: '测试站', station_area: '华东', total_sessions: 8, total_kwh: 40, total_fee: 35, utilization_rate: 55 }],
}

test('unified Flask envelope maps all ten ADS result groups', () => {
  const normalized = normalizeDashboard({ code: 0, data: groups })
  assert.equal(Object.keys(normalized).length, 10)
  assert.equal(normalized.overview.sessions, 100)
  const options = buildChartOptions(normalized)
  assert.equal(Object.keys(options).length, 9)
  assert.equal(options.hourly.series[0].data.length, 24)
  assert.equal(options.hourly.series[0].data[8], 12)
})

test('invalid or failed Flask response is never treated as real analytics data', () => {
  assert.throws(() => normalizeDashboard({ code: 50001, message: '数据库不可用' }), /数据库不可用/)
  assert.throws(() => normalizeDashboard({ code: 0, data: { overview: {} } }), /不是有效数字/)
})

test('all packaged OSM station IDs and coordinates remain valid', () => {
  const raw = JSON.parse(fs.readFileSync(new URL('../public/ads/stations.json', import.meta.url), 'utf8'))
  const normalized = normalizeStations(raw)
  assert.equal(normalized.stations.length, 3460)
  assert.equal(new Set(normalized.stations.map(station => station.id)).size, 3460)
  assert.equal(normalized.stations.filter(station => !station.coord).length, 0)
  assert.ok(normalized.stations.every(station => station.piles.length === 0))
})

test('missing numeric values remain null instead of fabricated zero', () => {
  const data = structuredClone(groups)
  data.user_radar[0].dim_value = null
  data.area_costs[0].profit_rate = null
  const normalized = normalizeDashboard({ code: 0, data })
  assert.equal(normalized.user_radar[0].dim_value, null)
  assert.equal(normalized.area_costs[0].profit_rate, null)
  assert.match(chartMissingReason('userRadar', normalized, {}), /无法完整归一化/)
})

test('missing optional source columns explain empty panels', () => {
  assert.match(chartMissingReason('platforms', groups, { quality: { platform_rows: 0 } }), /platform/)
  assert.match(chartMissingReason('battery', groups, { quality: { soc_rows: 0 } }), /SOC/)
})

test('source banner requires imported batch and unified script identity', () => {
  assert.equal(hasVerifiedSource({}), false)
  const source = { batch_id: 'test-batch', analysis: { module: 'analytics/scripts/evcharging_analysis.py', script_sha256: 'a'.repeat(64) } }
  assert.equal(hasVerifiedSource(source), true)
  source.analysis.module = 'other.py'
  assert.equal(hasVerifiedSource(source), false)
})
