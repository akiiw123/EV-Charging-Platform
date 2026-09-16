/**
 * 功能：在数据进入界面前校验 Flask 响应、数字、批次来源和 OSM 站点。
 * 输入：后端 JSON 与静态站点 JSON。
 * 输出/接口：normalizeDashboard/Metadata/Stations、hasVerifiedSource 和缺失原因；未知值保留 null。
 */
const isObject = value => value !== null && typeof value === 'object' && !Array.isArray(value)

function finite(value, field) {
  if (value === null || (typeof value === 'string' && value.trim() === '')) return null
  if (typeof value !== 'string' && typeof value !== 'number') throw new TypeError(`${field} 不是有效数字`)
  const number = Number(value)
  if (!Number.isFinite(number)) throw new TypeError(`${field} 不是有效数字`)
  return number
}

function rows(data, key) {
  if (!Array.isArray(data[key])) throw new TypeError(`${key} 数据缺失`)
  const numericFields = {
    user_levels: ['user_count'], user_radar: ['dim_value'], platforms: ['user_count'],
    hour_trend: ['hour', 'sessions', 'total_kwh', 'is_peak'],
    station_types: ['utilization_rate', 'daily_kwh', 'avg_fee_per_kwh'],
    week_compare: ['sessions', 'total_kwh', 'pct'], battery_health: ['sess_count', 'ratio'],
    area_costs: ['revenue', 'cost', 'profit', 'profit_rate'],
    top_stations: ['rn', 'total_sessions', 'total_kwh', 'total_fee', 'utilization_rate'],
    pile_status: ['pile_count'], pile_types: ['pile_count'],
  }
  return data[key].map(row => {
    if (!isObject(row)) throw new TypeError(`${key} 行格式不正确`)
    const normalized = { ...row }
    for (const field of numericFields[key] || []) normalized[field] = finite(row[field] ?? null, `${key}.${field}`)
    return normalized
  })
}

export function normalizeMetadata(value) {
  return isObject(value) ? value : {}
}

export function hasVerifiedSource(metadata) {
  const module = metadata?.analysis?.module
  const supported = module === 'analytics/scripts/evcharging_analysis.py'
    || (metadata?.metric_profile === 'ncs_teacher_dataset_v1'
      && module === 'ncs_data/spark_sql/03_ads_dashboard.sql')
  return typeof metadata?.batch_id === 'string' && metadata.batch_id.length > 0
    && supported
    && /^[a-f0-9]{64}$/.test(metadata.analysis?.script_sha256 || '')
}

const chartGroups = { userLevels: 'user_levels', userRadar: 'user_radar', platforms: 'platforms',
  battery: 'battery_health', hourly: 'hour_trend', stationTypes: 'station_types',
  weekCompare: 'week_compare', areaCosts: 'area_costs', topStations: 'top_stations', stationLoad: 'top_stations',
  pileStatus: 'pile_status', pileTypes: 'pile_types' }
export function chartMissingReason(key, data, metadata) {
  if (!data) return '等待可信分析结果'
  const group = data[chartGroups[key]] || []
  if (key === 'platforms' && metadata?.quality?.platform_rows === 0) return '原始订单缺少有效 platform，暂不能统计平台偏好'
  if (key === 'battery' && metadata?.quality?.soc_rows === 0) return '原始订单缺少有效起始 SOC，暂不能统计电量分布'
  if (key === 'topStations' && !group.some(row => row.total_sessions != null)) return '站点充电次数字段缺失，暂不能绘制排行'
  if (key === 'stationLoad' && !group.some(row => row.utilization_rate != null && row.total_fee != null)) return '站点负载或营收数据缺失，暂不能绘制关系图'
  if (!group.length) return '此维度没有可用的分析数据'
  if (key === 'userRadar' && group.some(row => row.dim_value == null)) return '存在无差异或缺失维度，无法完整归一化，不绘制为零'
  return ''
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
    pile_status: Array.isArray(data.pile_status) ? rows(data, 'pile_status') : [],
    pile_types: Array.isArray(data.pile_types) ? rows(data, 'pile_types') : [],
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

export function normalizeBusinessStations(payload) {
  if (!isObject(payload) || payload.code !== 0 || !isObject(payload.data)
      || payload.data.source !== 'platform_sqlite' || !Array.isArray(payload.data.stations)) {
    throw new TypeError(payload?.message || '业务站点响应格式不正确')
  }
  const ids = new Set()
  const stations = payload.data.stations.map(station => {
    if (!isObject(station) || station.id === undefined || ids.has(String(station.id))) {
      throw new TypeError('业务站点缺少唯一 ID')
    }
    ids.add(String(station.id))
    const longitude = finite(station.longitude, 'station.longitude')
    const latitude = finite(station.latitude, 'station.latitude')
    const coord = longitude != null && latitude != null
      && longitude >= -180 && longitude <= 180 && latitude >= -90 && latitude <= 90
      ? [longitude, latitude] : null
    const counts = { charging: 0, idle: 0, fault: 0, offline: 0, unknown: 0 }
    for (const key of ['charging', 'idle', 'fault', 'offline']) {
      counts[key] = finite(station.counts?.[key] ?? 0, `station.counts.${key}`)
    }
    return {
      id: String(station.id), name: String(station.name || '未命名站点'),
      province: String(station.province || ''), city: String(station.city || ''),
      district: String(station.district || ''), address: station.address || null,
      coord, counts, piles: [], status: station.status || null,
      attention: counts.fault + counts.offline,
      issues: [], lifecycle: null, operator: null, brand: null, access: null, sourceUrl: null,
    }
  })
  return { stations, snapshotAt: payload.data.generated_at || null, source: 'platform_sqlite' }
}
