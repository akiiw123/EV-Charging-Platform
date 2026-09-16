import test from 'node:test'
import assert from 'node:assert/strict'
import { buildPredictionModel } from '../src/lib/prediction-model.js'

test('prediction model identifies peak window and high-load stations from current aggregates', () => {
  const result = buildPredictionModel({
    hour_trend: Array.from({ length: 24 }, (_, hour) => ({ hour, sessions: hour === 8 ? 48 : 2, total_kwh: hour === 8 ? 120.5 : 3 })),
    top_stations: [
      { station_name: '高负载站', utilization_rate: 92 },
      { station_name: '普通站', utilization_rate: 55 },
    ],
  })
  assert.equal(result.available, true)
  assert.equal(result.peakWindow, '08:00–09:00')
  assert.equal(result.peakSessions, 48)
  assert.equal(result.peakKwh, 120.5)
  assert.equal(result.attentionCount, 1)
  assert.equal(result.leadStation, '高负载站')
  assert.equal(result.distribution.length, 24)
})

test('prediction model handles midnight rollover and unavailable data', () => {
  const midnight = buildPredictionModel({ hour_trend: [{ hour: 23, sessions: 4, total_kwh: 9 }], top_stations: [] })
  assert.equal(midnight.peakWindow, '23:00–次日 00:00')
  assert.equal(buildPredictionModel(null).available, false)
})
