import { normalizeDashboard, normalizeStations } from '../lib/dashboard-model.js'

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
  return normalizeDashboard(await getJson('/api/v1/dashboard', signal))
}

export async function fetchStations(signal) {
  const url = `${import.meta.env.BASE_URL}ads/stations.json`
  return normalizeStations(await getJson(url, signal))
}
