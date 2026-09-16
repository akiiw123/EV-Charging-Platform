/**
 * 功能：集中封装大屏的网络读取，不在 Vue 组件中散落 fetch 调用。
 * 输入：AbortSignal 和实时/批次模式；数据来自 Flask 大屏接口与实时业务站点接口。
 * 输出/接口：fetchDashboard 返回已校验的数据和来源信息，fetchStations 返回已校验的业务站点。
 */
import { normalizeBusinessStations, normalizeDashboard, normalizeMetadata } from '../lib/dashboard-model.js'

async function getJson(url, signal) {
  const response = await fetch(url, {
    cache: 'no-store',
    headers: { Accept: 'application/json' },
    signal,
  })
  if (!response.ok) throw new Error(`HTTP ${response.status}`)
  return response.json()
}

export async function fetchDashboard(signal, mode = 'batch') {
  const payload = await getJson(mode === 'live' ? '/api/v1/live/dashboard' : '/api/v1/dashboard', signal)
  return { data: normalizeDashboard(payload), metadata: normalizeMetadata(payload.metadata) }
}

export async function fetchStations(signal) {
  return normalizeBusinessStations(await getJson('/api/v1/live/stations', signal))
}
