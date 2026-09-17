/**
 * 功能：把十组标准分析数据转换成 ECharts 配置。
 * 输入：normalizeDashboard 的结果和 day/night 主题。
 * 输出/接口：buildChartOptions 返回饼图、雷达、柱线组合、散点和 TOP10 等 option。
 */
const number = value => value == null || value === '' ? null : Number.isFinite(Number(value)) ? Number(value) : null

const PALETTES = {
  night: {
    text: '#edf6ff', muted: '#afc3d6', grid: 'rgba(157, 187, 211, .22)',
    tooltip: '#20394f', colors: ['#1de9c4', '#48a8ff', '#f3c86a', '#a881ff', '#ff7c8a', '#5ed27b'],
  },
  day: {
    text: '#16334a', muted: '#668397', grid: 'rgba(41, 94, 127, .16)',
    tooltip: '#ffffff', colors: ['#078f80', '#197fc5', '#a86e00', '#7754c8', '#d74c61', '#2e9c50'],
  },
}

// 每张图共用的动画、字体、提示框和调色板，确保日夜主题一致切换。
function base(mode) {
  const palette = PALETTES[mode] || PALETTES.night
  return {
    animationDuration: 450,
    color: palette.colors,
    textStyle: { color: palette.text, fontFamily: 'Inter, "Microsoft YaHei", sans-serif' },
    tooltip: {
      trigger: 'axis',
      confine: true,
      backgroundColor: palette.tooltip,
      borderColor: palette.colors[0],
      textStyle: { color: palette.text, fontSize: 11 },
    },
    palette,
  }
}

function categoryAxes(mode, categories, unit = '') {
  const { palette } = base(mode)
  return {
    xAxis: {
      type: 'category', data: categories,
      axisLabel: { color: palette.muted, fontSize: 10, hideOverlap: true },
      axisLine: { lineStyle: { color: palette.grid } }, axisTick: { show: false },
    },
    yAxis: {
      type: 'value', name: unit, nameTextStyle: { color: palette.muted, fontSize: 9 },
      axisLabel: { color: palette.muted, fontSize: 9 }, axisLine: { show: false },
      splitLine: { lineStyle: { color: palette.grid, type: 'dashed' } },
    },
  }
}

function barStyle(color) {
  return { color, borderRadius: [3, 3, 0, 0] }
}

// 将 normalizeDashboard 的标准数据一次性转换为各卡片的 ECharts option。
// 此处只负责展示映射，不在前端重新定义后端业务指标口径。
export function buildChartOptions(data, mode = 'night') {
  if (!data) return {}
  const common = base(mode)
  const { palette } = common
  const legend = { textStyle: { color: palette.muted, fontSize: 9 }, itemWidth: 10, itemHeight: 6, top: 0 }
  const grid = { left: 42, right: 16, top: 36, bottom: 24 }

  const levelData = data.user_levels.map(row => ({ name: row.user_level, value: number(row.user_count) }))
  const platformData = data.platforms.map(row => ({ name: row.phone_type, value: number(row.user_count) }))
  const pileStatusLabels = { idle: '空闲', charging: '充电中', fault: '故障', offline: '离线' }
  const pileTypeLabels = { fast: '快充', slow: '慢充' }
  const pileStatusData = (data.pile_status || []).map(row => ({
    name: pileStatusLabels[row.pile_status] || row.pile_status, value: number(row.pile_count),
  }))
  const pileTypeData = (data.pile_types || []).map(row => ({
    name: pileTypeLabels[row.pile_type] || row.pile_type, value: number(row.pile_count),
  }))

  const radarDimensions = [...new Set(data.user_radar.map(row => row.dim_name))]
  const radarLevels = [...new Set(data.user_radar.map(row => row.user_level))]
  const radarMax = 100
  const radarSeries = radarLevels.map(level => ({
    name: level,
    value: radarDimensions.map(dimension => number(
      data.user_radar.find(row => row.user_level === level && row.dim_name === dimension)?.dim_value,
    )),
  }))

  const hours = Array.from({ length: 24 }, (_, hour) => hour)
  const hourRows = new Map(data.hour_trend.map(row => [number(row.hour), row]))
  const hourSessions = hours.map(hour => number(hourRows.get(hour)?.sessions))
  const hourKwh = hours.map(hour => number(hourRows.get(hour)?.total_kwh))

  const stationTypes = data.station_types.map(row => row.gun_type)
  const weekLabels = data.week_compare.map(row => row.day_type)
  const areaRows = [...data.area_costs].sort((a, b) => number(b.revenue) - number(a.revenue)).slice(0, 10).reverse()
  // Preserve the ADS ranking: sessions descending, then kWh descending.
  const topRows = [...data.top_stations].sort((a, b) => number(a.rn) - number(b.rn)).slice(0, 10)

  return {
    userLevels: {
      ...common,
      tooltip: { ...common.tooltip, trigger: 'item', formatter: '{b}<br/>{c} 人 · {d}%' },
      legend: { ...legend, bottom: 0, top: 'auto' },
      series: [{ type: 'pie', radius: ['43%', '70%'], center: ['50%', '45%'],
        label: { color: palette.text, fontSize: 10, formatter: '{b}\n{d}%' }, data: levelData }],
    },
    userRadar: {
      ...common, legend,
      radar: {
        center: ['50%', '56%'], radius: '59%', splitNumber: 4,
        indicator: radarDimensions.map(name => ({ name, max: radarMax })),
        axisName: { color: palette.muted, fontSize: 9 },
        splitLine: { lineStyle: { color: palette.grid } }, splitArea: { show: false },
        axisLine: { lineStyle: { color: palette.grid } },
      },
      series: [{ type: 'radar', data: radarSeries.filter(row => row.value.every(value => value !== null)), symbolSize: 3, areaStyle: { opacity: .12 } }],
    },
    platforms: {
      ...common,
      tooltip: { ...common.tooltip, trigger: 'item', formatter: '{b}<br/>{c} 人 · {d}%' },
      series: [{ type: 'pie', roseType: 'radius', radius: ['24%', '72%'], center: ['50%', '52%'],
        label: { color: palette.text, fontSize: 10 }, data: platformData }],
    },
    battery: {
      ...common, grid: { left: 74, right: 30, top: 18, bottom: 20 },
      xAxis: { type: 'value', max: 100, axisLabel: { color: palette.muted, formatter: '{value}%' },
        splitLine: { lineStyle: { color: palette.grid, type: 'dashed' } } },
      yAxis: { type: 'category', data: data.battery_health.map(row => row.health_level),
        axisLabel: { color: palette.muted, fontSize: 9 }, axisLine: { show: false }, axisTick: { show: false } },
      series: [{ type: 'bar', barWidth: 11, data: data.battery_health.map(row => number(row.ratio)),
        label: { show: true, position: 'right', color: palette.text, formatter: '{c}%' },
        itemStyle: { color: palette.colors[2], borderRadius: 6 } }],
    },
    pileStatus: {
      ...common,
      tooltip: { ...common.tooltip, trigger: 'item', formatter: '{b}<br/>{c} 个 · {d}%' },
      legend: { ...legend, bottom: 0, top: 'auto' },
      series: [{ type: 'pie', radius: ['42%', '70%'], center: ['50%', '45%'],
        label: { color: palette.text, fontSize: 10, formatter: '{b}\n{c} 个' }, data: pileStatusData }],
    },
    pileTypes: {
      ...common,
      tooltip: { ...common.tooltip, trigger: 'item', formatter: '{b}<br/>{c} 个 · {d}%' },
      series: [{ type: 'pie', roseType: 'radius', radius: ['24%', '72%'], center: ['50%', '52%'],
        label: { color: palette.text, fontSize: 10, formatter: '{b}\n{c} 个' }, data: pileTypeData }],
    },
    hourly: {
      ...common, legend, grid,
      ...categoryAxes(mode, hours.map(hour => `${String(hour).padStart(2, '0')}:00`)),
      yAxis: [categoryAxes(mode, []).yAxis, { type: 'value', name: 'kWh', position: 'right',
        axisLabel: { color: palette.muted, fontSize: 9 }, splitLine: { show: false } }],
      series: [
        { name: '充电次数', type: 'bar', barMaxWidth: 9, data: hourSessions, itemStyle: barStyle(palette.colors[1]) },
        { name: '充电量', type: 'line', yAxisIndex: 1, smooth: true, showSymbol: false, data: hourKwh,
          lineStyle: { width: 2, color: palette.colors[2] }, areaStyle: { color: `${palette.colors[2]}22` } },
      ],
    },
    stationTypes: {
      ...common, legend, grid,
      ...categoryAxes(mode, stationTypes),
      yAxis: [{ ...categoryAxes(mode, [], '%').yAxis, min: 0, max: 100 }, { type: 'value', name: '元/kWh', position: 'right',
        axisLabel: { color: palette.muted, fontSize: 9 }, splitLine: { show: false } }],
      series: [
        { name: '相对负载(%)', type: 'bar', barMaxWidth: 16, data: data.station_types.map(row => number(row.utilization_rate)),
          itemStyle: barStyle(palette.colors[0]) },
        { name: '结算收入/电量', type: 'line', yAxisIndex: 1, data: data.station_types.map(row => number(row.avg_fee_per_kwh)),
          symbolSize: 6, lineStyle: { color: palette.colors[3], width: 2 } },
      ],
    },
    weekCompare: {
      ...common, legend, grid,
      ...categoryAxes(mode, weekLabels),
      yAxis: [categoryAxes(mode, []).yAxis, { type: 'value', name: 'kWh', position: 'right',
        axisLabel: { color: palette.muted, fontSize: 9 }, splitLine: { show: false } }],
      series: [
        { name: '充电次数', type: 'bar', barWidth: 24, data: data.week_compare.map(row => number(row.sessions)), itemStyle: barStyle(palette.colors[0]) },
        { name: '充电量', type: 'bar', yAxisIndex: 1, barWidth: 24, data: data.week_compare.map(row => number(row.total_kwh)), itemStyle: barStyle(palette.colors[2]) },
      ],
    },
    areaCosts: {
      ...common, legend: { ...legend, top: 0 }, grid: { left: 72, right: 18, top: 34, bottom: 18 },
      xAxis: { type: 'value', axisLabel: { color: palette.muted, fontSize: 9 },
        splitLine: { lineStyle: { color: palette.grid, type: 'dashed' } } },
      yAxis: { type: 'category', data: areaRows.map(row => row.station_area),
        axisLabel: { color: palette.muted, fontSize: 9, width: 58, overflow: 'truncate' }, axisTick: { show: false } },
      series: [
        { name: '已结算营收', type: 'bar', data: areaRows.map(row => number(row.revenue)), itemStyle: { color: palette.colors[1] } },
        { name: '估算电量成本', type: 'bar', data: areaRows.map(row => number(row.cost)), itemStyle: { color: palette.colors[4] } },
        { name: '估算利润', type: 'bar', data: areaRows.map(row => number(row.profit)), itemStyle: { color: palette.colors[0] } },
      ],
    },
    stationLoad: {
      ...common, grid: { left: 65, right: 30, top: 30, bottom: 36 },
      tooltip: { ...common.tooltip, trigger: 'item' },
      xAxis: { type: 'value', name: '相对负载(%)', min: 0, max: 100, nameLocation: 'middle', nameGap: 23,
        axisLabel: { color: palette.muted }, splitLine: { lineStyle: { color: palette.grid } } },
      yAxis: { type: 'value', name: '已结算营收(元)', axisLabel: { color: palette.muted },
        splitLine: { lineStyle: { color: palette.grid, type: 'dashed' } } },
      series: [{ type: 'scatter', name: 'TOP10 站点', symbolSize: 12,
        dimensions: ['相对负载(%)', '已结算营收(元)'], encode: { x: 0, y: 1, tooltip: [0, 1] },
        data: topRows.filter(row => number(row.utilization_rate) !== null && number(row.total_fee) !== null)
          .map(row => ({ name: row.station_name, value: [number(row.utilization_rate), number(row.total_fee)] })),
        itemStyle: { color: palette.colors[0] } }],
    },
    topStations: {
      ...common, grid: { left: 110, right: 48, top: 18, bottom: 20 },
      xAxis: { type: 'value', name: '次', minInterval: 1, nameTextStyle: { color: palette.muted }, axisLabel: { color: palette.muted, fontSize: 9 },
        splitLine: { lineStyle: { color: palette.grid, type: 'dashed' } } },
      yAxis: { type: 'category', inverse: true, data: topRows.map(row => `${row.rn}. ${row.station_name}`),
        axisLabel: { color: palette.muted, fontSize: 9, width: 96, overflow: 'truncate' }, axisTick: { show: false } },
      series: [{ name: '充电次数', type: 'bar', data: topRows.map(row => number(row.total_sessions)),
        itemStyle: { color: palette.colors[2], borderRadius: [0, 5, 5, 0] },
        label: { show: true, position: 'right', color: palette.text, fontSize: 9 } }],
    },
  }
}
