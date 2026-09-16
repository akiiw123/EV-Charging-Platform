<script setup>
import { computed, nextTick, ref } from 'vue'
import { buildPredictionModel } from '../lib/prediction-model.js'

const props = defineProps({
  dashboard: { type: Object, default: null },
  mode: { type: String, default: 'live' },
})

const open = ref(false)
const orb = ref(null)
const panel = ref(null)
const prediction = computed(() => buildPredictionModel(props.dashboard))
const sourceLabel = computed(() => props.mode === 'live' ? '实时业务统计' : 'Spark 历史批次')
const maxSignal = computed(() => Math.max(...prediction.value.distribution, 1))

async function openPanel() {
  open.value = true
  await nextTick()
  panel.value?.focus()
}

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
          <span class="prediction-card__status"><i></i>规则推演</span>
          <button type="button" class="prediction-card__close" aria-label="收起智能预测" @click="closePanel">×</button>
        </header>

        <template v-if="prediction.available">
          <div class="prediction-card__lead">
            <span>预计高负荷窗口</span>
            <strong>{{ prediction.peakWindow }}</strong>
            <p>该时段充电会话强度约为全天均值的 {{ prediction.peakRatio.toFixed(1) }} 倍。</p>
          </div>

          <div class="prediction-card__metrics">
            <article>
              <span>高峰会话</span><strong>{{ prediction.peakSessions.toLocaleString('zh-CN') }}</strong><small>次</small>
            </article>
            <article>
              <span>参考负荷</span><strong>{{ prediction.peakKwh.toLocaleString('zh-CN', { maximumFractionDigits: 1 }) }}</strong><small>kWh</small>
            </article>
            <article>
              <span>高负载站点</span><strong>{{ prediction.attentionCount }}</strong><small>个</small>
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

        <div v-else class="prediction-card__empty">
          <strong>等待有效统计数据</strong>
          <span>数据加载完成后，将自动生成高峰时段与负荷提示。</span>
        </div>

        <footer class="prediction-card__footer">
          <span>数据源：{{ sourceLabel }}</span>
          <span>当前为统计规则推演，接入训练模型后可替换为 ML 预测结果</span>
        </footer>
      </section>
    </Transition>
  </Teleport>
</template>
