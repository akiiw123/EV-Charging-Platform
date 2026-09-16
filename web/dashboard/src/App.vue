<script setup>
import LiveOrders from './components/LiveOrders.vue'
import { Decoration5 } from '@kjgl77/datav-vue3'
import { computed, onBeforeUnmount, onMounted, provide, ref, watch } from 'vue'
import DashboardCard from './components/DashboardCard.vue'
import EChartPanel from './components/EChartPanel.vue'
import MapPanel from './components/MapPanel.vue'
import { fetchDashboard, fetchStations } from './api/analytics.js'
import { buildChartOptions } from './lib/chart-options.js'
import { chartMissingReason, hasVerifiedSource } from './lib/dashboard-model.js'

const chartGroup = ref('overview')
const chartDataMode = ref('live')
const isLive = computed(() => chartDataMode.value === 'live')
const dashboard = ref(null)
const metadata = ref({})
const assetBase = import.meta.env.BASE_URL
const stationData = ref(null)
const leftRailOpen = ref(false)
const rightRailOpen = ref(false)
const dashboardError = ref('')
const stationError = ref('')
const loading = ref(false)
const stationLoading = ref(false)
const updatedAt = ref(null)
const clock = ref(new Date())
const theme = ref(localStorage.getItem('voltflow-dashboard-theme') === 'day' ? 'day' : 'night')
provide('dashboardTheme', theme)
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
  { key: 'hourly', title: isLive.value ? '按开始小时统计（全部历史）' : '24 小时充电趋势', eyebrow: 'HOURLY TREND', badge: '次数 / 电量' },
  { key: 'stationTypes', title: '桩型充电负载', eyebrow: 'STATION TYPE', badge: '相对负载 / 单位收入' },
  { key: 'weekCompare', title: '工作日与周末', eyebrow: 'WEEK COMPARISON', badge: '双维对比' },
  { key: 'areaCosts', title: '区域收益与估算成本', eyebrow: 'AREA PROFIT', badge: 'TOP 10' },
])

const visibleLeftCharts = computed(() => leftCharts.value.slice(chartGroup.value === 'overview' ? 0 : 2, chartGroup.value === 'overview' ? 2 : 4))
const visibleRightCharts = computed(() => rightCharts.value.slice(chartGroup.value === 'overview' ? 0 : 2, chartGroup.value === 'overview' ? 2 : 4))

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

let railCloseTimer

function openRail(side) {
  window.clearTimeout(railCloseTimer)
  leftRailOpen.value = side === 'left'
  rightRailOpen.value = side === 'right'
}

function closeRails() {
  leftRailOpen.value = false
  rightRailOpen.value = false
}

function scheduleClose() {
  window.clearTimeout(railCloseTimer)
  railCloseTimer = window.setTimeout(closeRails, 280)
}

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
  clearTimeout(refreshTimer)
  refreshController?.abort()
  refreshController = new AbortController()
  const controller = refreshController
  const timeout = setTimeout(() => controller.abort(), 10000)
  loading.value = true
  try {
    const payload = await fetchDashboard(controller.signal, chartDataMode.value)
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
    if (refreshController === controller) {
      loading.value = false
      refreshTimer = setTimeout(refreshDashboard, isLive.value ? 5000 : 60000)
    }
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
  clockTimer = setInterval(() => { clock.value = new Date() }, 1000)
})

watch(chartDataMode, () => {
  dashboard.value = null
  metadata.value = {}
  updatedAt.value = null
  dashboardError.value = ''
  refreshDashboard()
})

onBeforeUnmount(() => {
  const pending = refreshController
  refreshController = null
  pending?.abort()
  stationController?.abort()
  clearTimeout(refreshTimer)
  clearInterval(clockTimer)
  clearTimeout(railCloseTimer)
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
        <p>看见充电网络，读懂每一次能量流动</p>
      </div>
      <div class="top-actions">
        <time>{{ clock.toLocaleString('zh-CN', { hour12: false }) }}</time>
        <button type="button" @click="toggleTheme">{{ theme === 'night' ? '日间' : '夜间' }}</button>
        <button type="button" :disabled="loading" @click="refreshDashboard">{{ loading ? '刷新中' : '刷新数据' }}</button>
      </div>
    </header>

    <LiveOrders />

    <div class="source-strip">
      <select v-model="chartDataMode" aria-label="图表数据来源">
        <option value="live">实时业务统计（5 秒更新）</option>
        <option value="batch">Spark 历史批次</option>
      </select>
      <span class="live-dot" :class="{ error: dashboardError, ok: hasDashboard }"></span>
      <strong>{{ dashboardError ? '统计接口异常' : hasDashboard ? (isLive ? '实时业务统计在线' : 'Spark 批次接口在线') : '正在连接统计接口' }}</strong>
      <span v-if="updatedAt">最近读取 {{ updatedAt.toLocaleTimeString('zh-CN', { hour12: false }) }}</span>
      <div class="analysis-switch" role="group" aria-label="分析图表分组">
        <button type="button" :aria-pressed="chartGroup === 'overview'" @click="chartGroup = 'overview'">用户与时段</button>
        <button type="button" :aria-pressed="chartGroup === 'structure'" @click="chartGroup = 'structure'">结构与收益</button>
      </div>
      <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener noreferrer">© OpenStreetMap contributors · ODbL</a>
    </div>

    <p v-if="dashboardError" class="error-banner" role="alert">
      Flask 数据读取失败：{{ dashboardError }}。页面保留最近一次成功结果，不使用伪造数据。
    </p>

    <section tabindex="0" class="kpi-grid" aria-label="核心指标">
      <DashboardCard v-for="item in kpis" :key="item.label" :title="item.label" eyebrow="CORE KPI" class="kpi-card">
        <div class="kpi-value"><strong>{{ item.value }}</strong><span>{{ item.unit }}</span></div>
      </DashboardCard>
    </section>

    <section
      class="analytics-grid"
      :class="{
        'left-open': leftRailOpen,
        'right-open': rightRailOpen
      }"
    >
      <aside
        class="chart-column chart-column--left"
        @mouseenter="openRail('left')"
        @mouseleave="scheduleClose"
        @focusin="openRail('left')"
        tabindex="0"
        @focusout="scheduleClose"
        @keydown.esc="closeRails"
        aria-label="左侧分析卡片"
      >
        <DashboardCard v-for="item in visibleLeftCharts" :key="item.key" :title="item.title" :eyebrow="item.eyebrow" :badge="item.badge">
          <EChartPanel :option="chartOptions[item.key] || {}" :empty="Boolean(missingReason(item.key))" :empty-message="missingReason(item.key)" />
        </DashboardCard>
      </aside>

      <div class="center-column">
  <MapPanel
    :station-data="stationData"
    :theme="theme"
    :loading="stationLoading"
    :error="stationError"
  />

  <div class="performance-docks" aria-label="站点绩效快捷分析">
    <DashboardCard
      title="站点充电次数 TOP10"
      eyebrow="SESSION RANKING"
      badge="按充电会话次数"
      tabindex="0"
      class="performance-dock performance-dock--left"
    >
      <EChartPanel
        :option="chartOptions.topStations || {}"
        :empty="Boolean(missingReason('topStations'))"
        :empty-message="missingReason('topStations')"
      />
    </DashboardCard>

    <DashboardCard
      title="站点负荷与收入关系"
      eyebrow="LOAD & REVENUE"
      badge="负载率 / 收入"
      tabindex="0"
      class="performance-dock performance-dock--right"
    >
      <EChartPanel
        :option="chartOptions.stationLoad || {}"
        :empty="Boolean(missingReason('stationLoad'))"
        :empty-message="missingReason('stationLoad')"
      />
    </DashboardCard>
  </div>
</div>

      <aside
        class="chart-column chart-column--right"
        @mouseenter="openRail('right')"
        @mouseleave="scheduleClose"
        @focusin="openRail('right')"
        tabindex="0"
        @focusout="scheduleClose"
        @keydown.esc="closeRails"
        aria-label="右侧分析卡片"
      >
        <DashboardCard v-for="item in visibleRightCharts" :key="item.key" :title="item.title" :eyebrow="item.eyebrow" :badge="item.badge">
          <EChartPanel :option="chartOptions[item.key] || {}" :empty="Boolean(missingReason(item.key))" :empty-message="missingReason(item.key)" />
        </DashboardCard>
      </aside>
    </section>

    <details class="metric-notes">
      <summary>指标口径与数据来源 · {{ isLive ? '业务数据库实时汇总' : verifiedSource ? '已提供分析批次' : '分析批次待核验' }}</summary>
      <div v-if="hasDashboard" class="analysis-source">
        <template v-if="isLive">
          <strong>业务数据库读取：{{ analysisTime }}</strong>
          <p>全部历史订单实时汇总 · 当前 {{ metadata.quality?.raw_count }} 笔订单 · 未运行 Spark 批次分析</p>
          <p>预约和取消不计充电会话；新预约只改变上方实时订单数，开始充电后才改变会话指标。</p>
          <p>按开始小时统计：全部历史会话按上海时间的开始小时分组，订单电量归入开始小时，不是最近 24 小时电表曲线。</p>
          <p>电量为订单已保存值；平台偏好、SOC 缺少原始字段，清洗剔除比例未计算，均不伪造。未配置成本单价时不展示估算成本和利润。</p>
        </template>
        <template v-else-if="verifiedSource">
          <strong>分析生成：{{ analysisTime }}</strong>
          <p>批次：{{ metadata.batch_id }} · 原始 {{ metadata.quality?.raw_count }} 条 / 有效 {{ metadata.quality?.valid_count }} 条 / 剔除 {{ metadata.quality?.rejected_count }} 条</p>
        </template>
        <p v-else>当前数据缺少新版分析批次信息，统计口径尚未核验，需完成新版分析结果导入。</p>
      </div>
      <ul>
        <li v-if="!isLive">以下为新版分析口径；无批次信息时，不能据此认定旧数据已更新。</li>
        <li>充电会话包含充电中、待结算与已完成订单；营收仅统计已完成订单金额及占位费。</li>
        <li v-if="!isLive">订单剔除比例：原始订单中被清洗、去重或关联校验剔除的比例，不是 SOC 缺失率。</li>
        <li>站点排行按充电次数、累计电量降序；右侧关系图使用同一 TOP10 站点，横轴为相对负载率，纵轴为已结算营收，不代表全体站点。</li>
        <li>相对负载率为充电次数除以同组最大次数，不代表设备时间利用率。起始 SOC 分布不代表电池健康状态。</li>
        <li>估算成本单价：{{ metadata.quality?.cost_per_kwh ?? '未知' }} 元/kWh；估算利润为营收减电量成本，未计设备和人工等成本。</li>
        <li>平台按下单用户去重，同一用户可出现在多个平台；周末按上海时区的周六、周日划分，不含调休。</li>
      </ul>
    </details>

    <footer>
      <span>{{ isLive ? "运营指标来自实时业务汇总" : "运营指标来自 Spark 分析批次" }} · 站点灯光仅表示位置</span>
      <span>地理资料：OSM 静态站点 {{ stationData?.stations.length?.toLocaleString('zh-CN') || 0 }} 条</span>
      <span>VOLTFlow V3 / 数据不全时明确显示未知</span>
      <span>估算成本单价：{{ metadata.quality?.cost_per_kwh ?? '未知' }} 元/kWh；周末指周六、周日</span>
    </footer>
  </main>
</template>
