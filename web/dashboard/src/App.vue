<script setup>
import { Decoration5 } from '@kjgl77/datav-vue3'
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import DashboardCard from './components/DashboardCard.vue'
import EChartPanel from './components/EChartPanel.vue'
import MapPanel from './components/MapPanel.vue'
import { fetchDashboard, fetchStations } from './api/analytics.js'
import { buildChartOptions } from './lib/chart-options.js'
import { chartMissingReason, hasVerifiedSource } from './lib/dashboard-model.js'

const dashboard = ref(null)
const metadata = ref({})
const assetBase = import.meta.env.BASE_URL
const stationData = ref(null)
const dashboardError = ref('')
const stationError = ref('')
const loading = ref(false)
const stationLoading = ref(false)
const updatedAt = ref(null)
const clock = ref(new Date())
const theme = ref(localStorage.getItem('voltflow-dashboard-theme') === 'day' ? 'day' : 'night')
let refreshController
let stationController
let refreshTimer
let clockTimer

const chartOptions = computed(() => buildChartOptions(dashboard.value, theme.value))
const hasDashboard = computed(() => Boolean(dashboard.value))
const verifiedSource = computed(() => hasVerifiedSource(metadata.value))
const missingReason = key => chartMissingReason(key, dashboard.value, metadata.value)
const analysisTime = computed(() => metadata.value.generated_at ? new Date(metadata.value.generated_at).toLocaleString('zh-CN', { hour12: false }) : '未知')

const leftCharts = computed(() => [
  { key: 'userLevels', title: '用户等级分布', eyebrow: 'USER SEGMENT', badge: `${dashboard.value?.user_levels.length || 0} 类` },
  { key: 'userRadar', title: '用户行为雷达', eyebrow: 'BEHAVIOR COMPARISON', badge: '多维对比' },
  { key: 'platforms', title: '终端平台偏好', eyebrow: 'PLATFORM SHARE', badge: `${dashboard.value?.platforms.length || 0} 类` },
  { key: 'battery', title: '起始 SOC 电量分布', eyebrow: 'STARTING SOC', badge: '非健康诊断' },
])

const rightCharts = computed(() => [
  { key: 'hourly', title: '24 小时充电趋势', eyebrow: 'HOURLY TREND', badge: '次数 / 电量' },
  { key: 'stationTypes', title: '桩型充电负载', eyebrow: 'STATION TYPE', badge: '相对负载 / 单位收入' },
  { key: 'weekCompare', title: '工作日与周末', eyebrow: 'WEEK COMPARISON', badge: '双维对比' },
  { key: 'areaCosts', title: '区域收益与估算成本', eyebrow: 'AREA PROFIT', badge: 'TOP 10' },
])

const kpis = computed(() => {
  const overview = dashboard.value?.overview
  return [
    { label: '充电会话', value: formatInteger(overview?.sessions), unit: '次' },
    { label: '累计充电量', value: formatDecimal(overview?.total_kwh), unit: 'kWh' },
    { label: '已结算营收（含占位费）', value: formatMoney(overview?.total_fee), unit: '元' },
    { label: '覆盖站点', value: formatInteger(overview?.station_count), unit: '站' },
    { label: '订单剔除比例', value: overview ? formatDecimal(overview.abnormal_rate) : '—', unit: '%' },
  ]
})

function formatInteger(value) {
  if (value == null || value === '') return '—'
  return Number.isFinite(Number(value)) ? Math.round(Number(value)).toLocaleString('zh-CN') : '—'
}

function formatDecimal(value) {
  if (value == null || value === '') return '—'
  return Number.isFinite(Number(value)) ? Number(value).toLocaleString('zh-CN', { maximumFractionDigits: 2 }) : '—'
}

function formatMoney(value) {
  if (value == null || value === '') return '—'
  return Number.isFinite(Number(value)) ? Number(value).toLocaleString('zh-CN', { minimumFractionDigits: 2, maximumFractionDigits: 2 }) : '—'
}

async function refreshDashboard() {
  refreshController?.abort()
  refreshController = new AbortController()
  const controller = refreshController
  const timeout = setTimeout(() => controller.abort(), 10000)
  loading.value = true
  try {
    const payload = await fetchDashboard(controller.signal)
    if (refreshController !== controller) return
    dashboard.value = payload.data
    metadata.value = payload.metadata
    dashboardError.value = ''
    updatedAt.value = new Date()
  } catch (error) {
    if (refreshController !== controller) return
    if (error.name !== 'AbortError') dashboardError.value = error.message
    else dashboardError.value = '分析接口请求超时'
  } finally {
    clearTimeout(timeout)
    if (refreshController === controller) loading.value = false
  }
}

async function loadStationData() {
  stationController?.abort()
  stationController = new AbortController()
  const timeout = setTimeout(() => stationController.abort(), 15000)
  stationLoading.value = true
  try {
    stationData.value = await fetchStations(stationController.signal)
    stationError.value = ''
  } catch (error) {
    stationError.value = error.name === 'AbortError' ? '站点静态资料加载超时' : error.message
  } finally {
    clearTimeout(timeout)
    stationLoading.value = false
  }
}

function toggleTheme() {
  theme.value = theme.value === 'night' ? 'day' : 'night'
}

watch(theme, value => {
  document.documentElement.dataset.theme = value
  localStorage.setItem('voltflow-dashboard-theme', value)
}, { immediate: true })

onMounted(() => {
  refreshDashboard()
  loadStationData()
  refreshTimer = setInterval(refreshDashboard, 60000)
  clockTimer = setInterval(() => { clock.value = new Date() }, 1000)
})

onBeforeUnmount(() => {
  refreshController?.abort()
  stationController?.abort()
  clearInterval(refreshTimer)
  clearInterval(clockTimer)
})
</script>

<template>
  <main class="dashboard-shell">
    <header class="topbar">
      <div class="brand">
        <img :src="`${assetBase}assets/voltflow-logo.png`" alt="VoltFlow 智充" />
        <div><strong>VoltFlow 智充</strong><small>ENERGY OPERATIONS</small></div>
      </div>
      <div class="title-block">
        <h1>充电网络大数据运营指挥台</h1>
        <Decoration5 class="title-decoration" :color="['#1de9c4', '#48a8ff']" :dur="3" />
        <p>SPARK SQL · MYSQL ADS · FLASK API · VUE 3 + DATAV</p>
      </div>
      <div class="top-actions">
        <time>{{ clock.toLocaleString('zh-CN', { hour12: false }) }}</time>
        <button type="button" @click="toggleTheme">{{ theme === 'night' ? '日间' : '夜间' }}</button>
        <button type="button" :disabled="loading" @click="refreshDashboard">{{ loading ? '刷新中' : '刷新数据' }}</button>
      </div>
    </header>

    <div class="source-strip">
      <span class="live-dot" :class="{ error: dashboardError, ok: hasDashboard }"></span>
      <strong>{{ dashboardError ? '分析接口异常' : hasDashboard ? '分析接口在线' : '正在连接分析接口' }}</strong>
      <span v-if="updatedAt">最近读取 {{ updatedAt.toLocaleTimeString('zh-CN', { hour12: false }) }}</span>
      <span>ADS 分析维度 10 组</span>
      <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener noreferrer">© OpenStreetMap contributors · ODbL</a>
    </div>

    <div v-if="hasDashboard" class="analysis-source" :class="{ unknown: !verifiedSource }">
      <template v-if="verifiedSource">
        <strong>数据来自统一 PySpark 分析</strong>
        <span>分析生成：{{ analysisTime }}</span>
        <span>批次：{{ metadata.batch_id }}</span>
        <span>原始 {{ metadata.quality?.raw_count }} 条 / 有效 {{ metadata.quality?.valid_count }} 条 / 剔除 {{ metadata.quality?.rejected_count }} 条</span>
        <details><summary>查看分析来源</summary><p>输入：{{ metadata.analysis?.raw_source }}</p><p>模块：{{ metadata.analysis?.module }}</p><p>脚本校验：{{ metadata.analysis?.script_sha256 }}</p><p>分析编号：{{ metadata.analysis?.analysis_id }}</p></details>
      </template>
      <template v-else>接口返回了数据，但没有统一分析的批次信息；不能确认数据来自你的新版脚本。请完成新版 ADS 导入。</template>
    </div>

    <p v-if="dashboardError" class="error-banner" role="alert">
      Flask 数据读取失败：{{ dashboardError }}。页面保留最近一次成功结果，不使用伪造数据。
    </p>

    <section class="kpi-grid" aria-label="核心指标">
      <DashboardCard v-for="item in kpis" :key="item.label" :title="item.label" eyebrow="CORE KPI" class="kpi-card">
        <div class="kpi-value"><strong>{{ item.value }}</strong><span>{{ item.unit }}</span></div>
      </DashboardCard>
    </section>

    <section class="analytics-grid">
      <aside class="chart-column">
        <DashboardCard v-for="item in leftCharts" :key="item.key" :title="item.title" :eyebrow="item.eyebrow" :badge="item.badge">
          <EChartPanel :option="chartOptions[item.key] || {}" :empty="Boolean(missingReason(item.key))" :empty-message="missingReason(item.key)" />
        </DashboardCard>
      </aside>

      <div class="center-column">
        <MapPanel :station-data="stationData" :theme="theme" :loading="stationLoading" :error="stationError" />
        <DashboardCard title="充电次数 TOP10 站点的营收" eyebrow="STATION PERFORMANCE" badge="按营收展示" class="ranking-card">
          <EChartPanel :option="chartOptions.topStations || {}" :empty="Boolean(missingReason('topStations'))" :empty-message="missingReason('topStations')" />
        </DashboardCard>
      </div>

      <aside class="chart-column">
        <DashboardCard v-for="item in rightCharts" :key="item.key" :title="item.title" :eyebrow="item.eyebrow" :badge="item.badge">
          <EChartPanel :option="chartOptions[item.key] || {}" :empty="Boolean(missingReason(item.key))" :empty-message="missingReason(item.key)" />
        </DashboardCard>
      </aside>
    </section>

    <footer>
      <span>运营数据：Spark SQL → MySQL `charging_ads` → Flask</span>
      <span>地理资料：OSM 静态站点 {{ stationData?.stations.length?.toLocaleString('zh-CN') || 0 }} 条</span>
      <span>VOLTFlow V3 / 数据不全时明确显示未知</span>
      <span>估算成本单价：{{ metadata.quality?.cost_per_kwh ?? '未知' }} 元/kWh；周末指周六、周日</span>
    </footer>
  </main>
</template>
