import { normalizeDashboard, normalizeMetadata, normalizeStations } from '../lib/dashboard-model.js'

async function getJson(url, signal) {
  const response = await fetch(url, {
    cache: 'no-store',
    headers: { Accept: 'application/json' },
    signal,
  })
  if (!response.ok) throw new Error(`HTTP ${response.status}`)
  return response.json()
}

export async function fetchDashboard(signal) {
  const payload = await getJson('/api/v1/dashboard', signal)
  return { data: normalizeDashboard(payload), metadata: normalizeMetadata(payload.metadata) }
}

export async function fetchStations(signal) {
  const url = `${import.meta.env.BASE_URL}ads/stations.json`
  return normalizeStations(await getJson(url, signal))
}
