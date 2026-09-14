const number = value => Number(value) || 0

const PALETTES = {
  night: {
    text: '#dcecff', muted: '#7896b7', grid: 'rgba(80, 126, 168, .18)',
    tooltip: '#07182b', colors: ['#1de9c4', '#48a8ff', '#f3c86a', '#a881ff', '#ff7c8a', '#5ed27b'],
  },
  day: {
    text: '#16334a', muted: '#668397', grid: 'rgba(41, 94, 127, .16)',
    tooltip: '#ffffff', colors: ['#078f80', '#197fc5', '#a86e00', '#7754c8', '#d74c61', '#2e9c50'],
  },
}

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

export function buildChartOptions(data, mode = 'night') {
  if (!data) return {}
  const common = base(mode)
  const { palette } = common
  const legend = { textStyle: { color: palette.muted, fontSize: 9 }, itemWidth: 10, itemHeight: 6, top: 0 }
  const grid = { left: 42, right: 16, top: 36, bottom: 24 }

  const levelData = data.user_levels.map(row => ({ name: row.user_level, value: number(row.user_count) }))
  const platformData = data.platforms.map(row => ({ name: row.phone_type, value: number(row.user_count) }))

  const radarDimensions = [...new Set(data.user_radar.map(row => row.dim_name))]
  const radarLevels = [...new Set(data.user_radar.map(row => row.user_level))]
  const radarMax = Math.max(1, ...data.user_radar.map(row => number(row.dim_value)))
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
  const topRows = [...data.top_stations].sort((a, b) => number(a.total_fee) - number(b.total_fee)).slice(-10)

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
        indicator: radarDimensions.map(name => ({ name, max: Math.ceil(radarMax * 1.1) })),
        axisName: { color: palette.muted, fontSize: 9 },
        splitLine: { lineStyle: { color: palette.grid } }, splitArea: { show: false },
        axisLine: { lineStyle: { color: palette.grid } },
      },
      series: [{ type: 'radar', data: radarSeries, symbolSize: 3, areaStyle: { opacity: .12 } }],
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
      yAxis: [categoryAxes(mode, []).yAxis, { type: 'value', name: '元/kWh', position: 'right',
        axisLabel: { color: palette.muted, fontSize: 9 }, splitLine: { show: false } }],
      series: [
        { name: '利用率', type: 'bar', barMaxWidth: 16, data: data.station_types.map(row => number(row.utilization_rate)),
          itemStyle: barStyle(palette.colors[0]) },
        { name: '平均电价', type: 'line', yAxisIndex: 1, data: data.station_types.map(row => number(row.avg_fee_per_kwh)),
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
        { name: '营收', type: 'bar', data: areaRows.map(row => number(row.revenue)), itemStyle: { color: palette.colors[1] } },
        { name: '成本', type: 'bar', data: areaRows.map(row => number(row.cost)), itemStyle: { color: palette.colors[4] } },
        { name: '利润', type: 'bar', data: areaRows.map(row => number(row.profit)), itemStyle: { color: palette.colors[0] } },
      ],
    },
    topStations: {
      ...common, grid: { left: 110, right: 28, top: 18, bottom: 20 },
      xAxis: { type: 'value', axisLabel: { color: palette.muted, fontSize: 9 },
        splitLine: { lineStyle: { color: palette.grid, type: 'dashed' } } },
      yAxis: { type: 'category', data: topRows.map(row => row.station_name),
        axisLabel: { color: palette.muted, fontSize: 9, width: 96, overflow: 'truncate' }, axisTick: { show: false } },
      series: [{ name: '累计金额', type: 'bar', data: topRows.map(row => number(row.total_fee)),
        itemStyle: { color: palette.colors[2], borderRadius: [0, 5, 5, 0] },
        label: { show: true, position: 'right', color: palette.text, fontSize: 9 } }],
    },
  }
}
