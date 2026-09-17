/** Build a transparent rule-based forecast from the dashboard's current aggregates. */
// 从当前标准大屏数据提取规则预测输入；仅用于解释性辅助，不替代独立 ML 服务结果。
export function buildPredictionModel(dashboard) {
  const hourly = Array.isArray(dashboard?.hour_trend) ? dashboard.hour_trend : []
  const validHours = hourly
    .map(row => ({
      hour: Number(row.hour),
      sessions: Number(row.sessions),
      totalKwh: Number(row.total_kwh),
    }))
    .filter(row => Number.isInteger(row.hour) && row.hour >= 0 && row.hour <= 23
      && Number.isFinite(row.sessions) && row.sessions >= 0
      && Number.isFinite(row.totalKwh) && row.totalKwh >= 0)

  if (!validHours.length || !validHours.some(row => row.sessions > 0)) {
    return { available: false, distribution: Array.from({ length: 24 }, () => 0) }
  }

  const peak = validHours.reduce((best, row) => row.sessions > best.sessions ? row : best)
  const totalSessions = validHours.reduce((sum, row) => sum + row.sessions, 0)
  const meanSessions = totalSessions / 24
  const distribution = Array.from({ length: 24 }, (_, hour) => validHours.find(row => row.hour === hour)?.sessions ?? 0)
  const topStations = Array.isArray(dashboard?.top_stations) ? dashboard.top_stations : []
  const comparableStations = topStations.filter(row => Number.isFinite(Number(row.utilization_rate)))
  const attentionStations = comparableStations.filter(row => Number(row.utilization_rate) >= 80)
  const leadStation = comparableStations.reduce(
    (best, row) => !best || Number(row.utilization_rate) > Number(best.utilization_rate) ? row : best,
    null,
  )
  const endHour = (peak.hour + 1) % 24

  return {
    available: true,
    peakWindow: `${String(peak.hour).padStart(2, '0')}:00–${endHour === 0 ? '次日 ' : ''}${String(endHour).padStart(2, '0')}:00`,
    peakSessions: peak.sessions,
    peakKwh: peak.totalKwh,
    peakRatio: meanSessions > 0 ? peak.sessions / meanSessions : 0,
    attentionCount: attentionStations.length,
    leadStation: leadStation?.station_name || null,
    distribution,
  }
}
