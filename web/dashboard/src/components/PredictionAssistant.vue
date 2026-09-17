<script setup>
import { computed, nextTick, onBeforeUnmount, ref } from 'vue'
import { fetchMlPrediction, fetchMlStations } from '../api/analytics.js'
import { buildPredictionModel } from '../lib/prediction-model.js'

const props = defineProps({
  dashboard: { type: Object, default: null },
  mode: { type: String, default: 'live' },
})

const open = ref(false)
const orb = ref(null)
const panel = ref(null)
const fallbackPrediction = computed(() => buildPredictionModel(props.dashboard))
const mlState = ref('idle')
const mlError = ref('')
const mlStations = ref([])
const selectedStationId = ref('')
const mlPrediction = ref(null)
let controller = null

const usingMl = computed(() => mlState.value === 'ready' && mlPrediction.value)
const prediction = computed(() => usingMl.value ? normalizeMlPrediction(mlPrediction.value) : fallbackPrediction.value)
const sourceLabel = computed(() => props.mode === 'live' ? '实时业务统计' : 'Spark 历史批次')
const maxSignal = computed(() => Math.max(...prediction.value.distribution, 1))
const statusLabel = computed(() => ({
  idle: '等待连接', loading: '模型加载中', ready: '历史预测', unavailable: '历史估算', failed: '推理失败',
})[mlState.value])

function hourText(value) {
  const date = new Date(value)
  return Number.isNaN(date.getTime()) ? '--:--' : date.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', hour12: false })
}

// 校验 ML 代理响应并整理 1/6/24 小时结果；缺少真实结果时抛错，不伪造预测值。
function normalizeMlPrediction(payload) {
  const curve = Array.isArray(payload?.curve) ? payload.curve : []
  const distribution = curve.map((row) => Number(row.load_kwh) || 0)
  const peak = distribution.length ? Math.max(...distribution) : 0
  const peakIndex = distribution.indexOf(peak)
  const peakRow = curve[peakIndex] || {}
  const total = distribution.reduce((sum, value) => sum + value, 0)
  return {
    available: curve.length > 0,
    distribution,
    peakWindow: `${hourText(peakRow.interval_start)}–${hourText(peakRow.interval_end)}`,
    peakRatio: peak / Math.max(total / Math.max(distribution.length, 1), 0.001),
    peakSessions: Math.max(0, (Number(payload.total_piles) || 0) * (1 - (Number(peakRow.busy_ratio) || 0))),
    peakKwh: peak,
    attentionCount: Number(payload.total_piles) || 0,
    leadStation: payload.station_name || payload.station_id || '',
  }
}

// 取消上一站请求后加载当前站点预测，防止快速切换站点造成旧响应覆盖新状态。
async function loadPrediction() {
  controller?.abort()
  controller = new AbortController()
  mlState.value = 'loading'
  mlError.value = ''
  mlPrediction.value = null
  let stationsLoaded = false
  try {
    const listing = await fetchMlStations(controller.signal)
    mlStations.value = Array.isArray(listing.stations) ? listing.stations : []
    stationsLoaded = true
    if (!mlStations.value.length) throw new Error('模型没有可回放站点')
    if (!mlStations.value.some((item) => String(item.station_id) === selectedStationId.value)) {
      selectedStationId.value = String(mlStations.value[0].station_id)
    }
    mlPrediction.value = await fetchMlPrediction(selectedStationId.value, controller.signal)
    mlState.value = 'ready'
  } catch (error) {
    if (error.name === 'AbortError') return
    mlError.value = error.message || '模型服务不可用'
    mlState.value = stationsLoaded ? 'failed' : 'unavailable'
  }
}

// 首次展开先获取可预测站点目录，再按默认站点加载预测。
async function openPanel() {
  open.value = true
  if (mlState.value === 'idle') loadPrediction()
  await nextTick()
  panel.value?.focus()
}

onBeforeUnmount(() => controller?.abort())

async function closePanel() {
  open.value = false
  await nextTick()
  orb.value?.focus()
}

function togglePanel() {
  if (open.value) closePanel()
  else openPanel()
}
</script>

<template>
  <button
    ref="orb"
    type="button"
    class="prediction-orb"
    :class="{ active: open }"
    :aria-expanded="open"
    aria-controls="predictionPanel"
    :aria-label="open ? '收起智能预测' : '打开智能预测'"
    title="智能预测"
    @click.stop="togglePanel"
  >
    <svg viewBox="0 0 48 48" aria-hidden="true">
      <path class="prediction-orb__ring" d="M24 4a20 20 0 1 1-14.14 5.86" />
      <rect x="12" y="15" width="24" height="20" rx="8" />
      <path d="M24 10v5M21 9h6M17 37h14" />
      <circle cx="19" cy="24" r="2" /><circle cx="29" cy="24" r="2" />
      <path d="M19 30c3 2 7 2 10 0" />
    </svg>
    <span class="prediction-orb__pulse"></span>
  </button>

  <Teleport to="body">
    <Transition name="prediction-panel">
      <section
        v-if="open"
        id="predictionPanel"
        ref="panel"
        class="prediction-card"
        role="region"
        aria-label="智能预测卡片"
        tabindex="-1"
        @keydown.esc.stop="closePanel"
      >
        <header class="prediction-card__header">
          <div>
            <small>VOLT AI · OPERATIONS FORECAST</small>
            <h3>智能预测</h3>
          </div>
          <span class="prediction-card__status"><i></i>{{ statusLabel }}</span>
          <button type="button" class="prediction-card__close" aria-label="收起智能预测" @click="closePanel">×</button>
        </header>

        <div v-if="mlStations.length" class="prediction-card__station">
          <label for="predictionStation">预测站点</label>
          <select id="predictionStation" v-model="selectedStationId" :disabled="mlState === 'loading'" @change="loadPrediction">
            <option v-for="station in mlStations" :key="station.station_id" :value="String(station.station_id)">
              {{ station.station_name || station.station_id }}
            </option>
          </select>
        </div>

        <template v-if="prediction.available && mlState !== 'failed'">
          <div class="prediction-card__lead">
            <span>预计高负荷窗口</span>
            <strong>{{ prediction.peakWindow }}</strong>
            <p>{{ usingMl ? '历史回放模型' : '统计规则' }}显示该时段负荷约为全天均值的 {{ prediction.peakRatio.toFixed(1) }} 倍。</p>
          </div>

          <div class="prediction-card__metrics">
            <article>
              <span>{{ usingMl ? '预计空闲桩' : '高峰会话' }}</span><strong>{{ prediction.peakSessions.toLocaleString('zh-CN') }}</strong><small>{{ usingMl ? '个' : '次' }}</small>
            </article>
            <article>
              <span>峰值小时电量</span><strong>{{ prediction.peakKwh.toLocaleString('zh-CN', { maximumFractionDigits: 1 }) }}</strong><small>kWh</small>
            </article>
            <article>
              <span>{{ usingMl ? '站点充电桩' : '高负载站点' }}</span><strong>{{ prediction.attentionCount }}</strong><small>个</small>
            </article>
          </div>

          <div class="prediction-signal" aria-label="24 小时会话分布">
            <div class="prediction-signal__head">
              <span>24H 负荷轮廓</span><small>{{ prediction.leadStation ? `重点：${prediction.leadStation}` : '暂无可比较站点' }}</small>
            </div>
            <div class="prediction-signal__bars" aria-hidden="true">
              <i v-for="(value, hour) in prediction.distribution" :key="hour" :style="{ '--signal': `${Math.max(6, value / maxSignal * 100)}%` }"></i>
            </div>
            <div class="prediction-signal__axis"><span>00</span><span>06</span><span>12</span><span>18</span><span>24</span></div>
          </div>
        </template>

        <div v-else-if="mlState === 'loading'" class="prediction-card__empty">
          <strong>正在连接模型服务</strong><span>加载可回放站点和 24 小时预测。</span>
        </div>

        <div v-else class="prediction-card__empty">
          <strong>{{ mlState === 'failed' ? '模型推理失败' : '等待有效统计数据' }}</strong>
          <span>{{ mlError || '数据加载完成后，将自动生成高峰时段与负荷提示。' }}</span>
          <button v-if="mlState === 'failed'" type="button" @click="loadPrediction">重新预测</button>
        </div>

        <footer class="prediction-card__footer">
          <span>数据源：{{ usingMl ? '2025 年历史回放模型' : sourceLabel }}</span>
          <span v-if="usingMl">日期经历史平移；结果为 seasonal 季节性模型，不代表实时可预约桩数</span>
          <span v-else-if="mlState === 'unavailable'">模型服务不可用：{{ mlError }}；当前显示统计规则估算</span>
          <span v-else>模型结果不可用时不会沿用旧预测值</span>
        </footer>
      </section>
    </Transition>
  </Teleport>
</template>
