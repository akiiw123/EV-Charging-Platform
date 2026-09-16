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
