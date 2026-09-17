<!--
功能：展示业务库中的实时订单概况和最近十笔订单，每 5 秒自动刷新。
输入：GET /api/v1/live/orders 的统一 JSON 响应。
输出/接口：状态卡片、订单表格、手动刷新、超时和“保留上次成功结果”提示；组件本身不修改订单。
-->
<script setup>
import { computed, onMounted, onBeforeUnmount, ref } from 'vue'

const data = ref(null)
const error = ref('')
const loading = ref(false)
const labels = { reserved: '已预约', charging: '充电中', awaiting_payment: '待结算', completed: '已完成', cancelled: '已取消' }
let timer, controller, stopped = false
const number = value => Number(value).toLocaleString('zh-CN', { maximumFractionDigits: 2 })
const cards = computed(() => data.value ? [
  ['订单总数', data.value.total_orders],
  ['已预约', data.value.counts.reserved],
  ['充电中', data.value.counts.charging],
  ['待结算', data.value.counts.awaiting_payment],
  ['已完成', data.value.counts.completed],
  ['已取消', data.value.counts.cancelled],
  ['已记录电量（kWh）', data.value.recorded_energy_kwh],
  ['已结算营收（元）', data.value.settled_revenue],
] : [])

// 周期读取最新订单；组件卸载或新一轮开始时取消旧请求，避免竞态和资源泄漏。
async function refresh() {
  if (loading.value || stopped) return
  clearTimeout(timer)
  loading.value = true
  controller = new AbortController()
  const timeout = setTimeout(() => controller.abort(), 8000)
  try {
    const response = await fetch('/api/v1/live/orders', { signal: controller.signal, cache: 'no-store' })
    if (!response.ok) throw new Error(`实时订单接口 HTTP ${response.status}`)
    const result = await response.json()
    if (result.code !== 0 || result.data?.source !== 'platform_sqlite') throw new Error('实时订单响应无效')
    if (!stopped) { data.value = result.data; error.value = '' }
  } catch (problem) {
    if (!stopped) error.value = problem.name === 'AbortError' ? '实时订单请求超时' : problem.message
  } finally {
    clearTimeout(timeout)
    loading.value = false
    if (!stopped) timer = setTimeout(refresh, 5000)
  }
}
onMounted(refresh)
onBeforeUnmount(() => { stopped = true; clearTimeout(timer); controller?.abort() })
</script>

<template>
  <section class="live-orders" aria-label="实时业务订单">
    <header>
      <strong>实时业务订单</strong>
      <span>每 5 秒读取 · 全部订单</span>
      <span v-if="data">{{ error ? '上次成功读取' : '最近读取' }} {{ new Date(data.generated_at).toLocaleTimeString('zh-CN', { hour12: false }) }}</span>
      <button type="button" :disabled="loading" @click="refresh">{{ loading ? '读取中' : '刷新订单' }}</button>
    </header>
    <p v-if="error" class="live-error" role="alert">{{ error }}。{{ data ? '以下为上次成功结果，当前数据可能已变化。' : '尚未取得实时数据。' }}</p>
    <p v-else-if="!data">正在连接业务订单…</p>
    <div v-if="data" class="live-cards" :class="{ stale: error }">
      <div v-for="[label, value] in cards" :key="label"><span>{{ label }}</span><strong>{{ number(value) }}</strong></div>
    </div>
    <details v-if="data">
      <summary>最近创建的 10 笔订单（状态随刷新更新）</summary>
      <div class="live-table">
        <table>
          <thead><tr><th>订单号</th><th>站点</th><th>桩编号</th><th>状态</th><th>已记录电量 kWh</th><th>已结算金额 元</th></tr></thead>
          <tbody><tr v-for="order in data.recent_orders" :key="order.id">
            <td>{{ order.id }}</td><td>{{ order.station_name }}</td><td>{{ order.pile_code }}</td>
            <td>{{ labels[order.status] || order.status }}</td><td>{{ number(order.energy_kwh) }}</td>
            <td>{{ order.settled_amount == null ? '—' : number(order.settled_amount) }}</td>
          </tr></tbody>
        </table>
        <p v-if="!data.recent_orders.length">暂无订单</p>
      </div>
    </details>
    <small>电量来自订单已保存值，非实时电表；营收仅计已完成订单（含占位费）。下方图表可选择实时统计或 Spark 历史批次，全国地图为静态站点。</small>
  </section>
</template>

<style scoped>
.live-orders { flex: none; position: relative; z-index: 2; pointer-events: auto; padding: 10px 14px; border: 1px solid var(--border); border-radius: 10px; background: var(--panel); color: var(--text); }
header { display: flex; align-items: center; flex-wrap: wrap; gap: 12px; }
header span, small { color: var(--muted); font-size: 11px; }
header button { margin-left: auto; }
.live-cards { display: grid; grid-template-columns: repeat(8, minmax(0, 1fr)); gap: 10px; margin: 8px 0; }
.live-cards span { display: block; font-size: 11px; color: var(--muted); }
.live-cards strong { display: block; font-size: 22px; color: var(--accent); }
.live-error { color: var(--danger); }
.stale { opacity: .65; }
details { margin: 8px 0; }
summary { cursor: pointer; }
.live-table { max-height: 210px; overflow: auto; }
table { width: 100%; border-collapse: collapse; text-align: left; }
th, td { padding: 6px; border-bottom: 1px solid var(--border); font-size: 12px; }
@media (max-width: 1000px) { .live-cards { grid-template-columns: repeat(4, minmax(0, 1fr)); } }
</style>
