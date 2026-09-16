/**
 * 功能：集中封装大屏的网络读取，不在 Vue 组件中散落 fetch 调用。
 * 输入：AbortSignal 和实时/批次模式；数据来自 Flask 大屏接口及对应模式的站点接口。
 * 输出/接口：fetchDashboard 返回已校验的数据和来源信息，fetchStations 返回已校验的站点。
 */
import { normalizeBusinessStations, normalizeDashboard, normalizeMetadata } from '../lib/dashboard-model.js'

async function getJson(url, signal) {
  const response = await fetch(url, {
    cache: 'no-store',
    headers: { Accept: 'application/json' },
    signal,
  })
  const payload = await response.json().catch(() => ({}))
  if (!response.ok) throw new Error(payload.error || payload.message || `HTTP ${response.status}`)
  return payload
}

export async function fetchDashboard(signal, mode = 'batch') {
  const payload = await getJson(mode === 'live' ? '/api/v1/live/dashboard' : '/api/v1/dashboard', signal)
  return { data: normalizeDashboard(payload), metadata: normalizeMetadata(payload.metadata) }
}

export async function fetchStations(signal, mode = 'live') {
  const url = mode === 'live' ? '/api/v1/live/stations' : '/api/v1/stations/map'
  return normalizeBusinessStations(await getJson(url, signal))
}

export async function fetchMlStations(signal) {
  return getJson('/api/v1/ml/stations', signal)
}

export async function fetchMlPrediction(stationId, signal) {
  const response = await fetch('/api/v1/ml/predict', {
    method: 'POST',
    cache: 'no-store',
    headers: { Accept: 'application/json', 'Content-Type': 'application/json' },
    body: JSON.stringify({ station_id: stationId, horizons: [1, 6, 24] }),
    signal,
  })
  const payload = await response.json().catch(() => ({}))
  if (!response.ok) throw new Error(payload.error || payload.message || `HTTP ${response.status}`)
  return payload
}
