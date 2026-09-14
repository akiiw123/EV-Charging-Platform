const isObject = value => value !== null && typeof value === 'object' && !Array.isArray(value)

function finite(value, field) {
  const number = Number(value)
  if (!Number.isFinite(number)) throw new TypeError(`${field} 不是有效数字`)
  return number
}

function rows(data, key) {
  if (!Array.isArray(data[key])) throw new TypeError(`${key} 数据缺失`)
  return data[key]
}

export function normalizeDashboard(payload) {
  if (!isObject(payload) || payload.code !== 0 || !isObject(payload.data)) {
    throw new TypeError(payload?.message || '分析接口响应格式不正确')
  }
  const data = payload.data
  if (!isObject(data.overview)) throw new TypeError('overview 数据缺失')
  const overview = {
    sessions: finite(data.overview.sessions, 'sessions'),
    total_kwh: finite(data.overview.total_kwh, 'total_kwh'),
    total_fee: finite(data.overview.total_fee, 'total_fee'),
    station_count: finite(data.overview.station_count, 'station_count'),
    abnormal_rate: finite(data.overview.abnormal_rate, 'abnormal_rate'),
  }
  return {
    overview,
    user_levels: rows(data, 'user_levels'),
    user_radar: rows(data, 'user_radar'),
    platforms: rows(data, 'platforms'),
    hour_trend: rows(data, 'hour_trend'),
    station_types: rows(data, 'station_types'),
    week_compare: rows(data, 'week_compare'),
    battery_health: rows(data, 'battery_health'),
    area_costs: rows(data, 'area_costs'),
    top_stations: rows(data, 'top_stations'),
  }
}

export function normalizeStations(payload) {
  if (!isObject(payload) || payload.data_kind !== 'static_station_inventory' || !Array.isArray(payload.stations)) {
    throw new TypeError('站点 ADS 格式不正确')
  }
  const ids = new Set()
  const stations = payload.stations.map(station => {
    if (!isObject(station) || station.id === undefined || ids.has(String(station.id))) {
      throw new TypeError('站点缺少唯一 ID')
    }
    ids.add(String(station.id))
    const longitude = Number(station.longitude)
    const latitude = Number(station.latitude)
    const coord = Number.isFinite(longitude) && Number.isFinite(latitude)
      && longitude >= -180 && longitude <= 180 && latitude >= -90 && latitude <= 90
      ? [longitude, latitude]
      : null
    return {
      id: String(station.id),
      name: String(station.name || '未命名站点'),
      province: String(station.province || ''),
      city: String(station.city || ''),
      coord,
      piles: [],
      counts: { charging: 0, idle: 0, fault: 0, offline: 0, unknown: 0 },
      attention: 0,
      issues: Array.isArray(station.issues) ? station.issues : [],
      lifecycle: station.lifecycle || null,
      operator: station.operator || null,
      brand: station.brand || null,
      address: station.address || null,
      access: station.access || null,
      sourceUrl: station.source_url || null,
    }
  })
  return { stations, snapshotAt: payload.snapshot_at || null }
}
