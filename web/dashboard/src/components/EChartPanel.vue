<!--
  功能：统一创建、更新、缩放和销毁 ECharts 实例。
  输入：option 图表配置、empty 状态和空状态说明。
  输出/接口：按需支持柱、线、饼、雷达、散点图；容器变化时自动 resize。
-->
<script setup>
import * as echarts from 'echarts/core'
import { BarChart, LineChart, PieChart, RadarChart, ScatterChart } from 'echarts/charts'
import { GridComponent, LegendComponent, RadarComponent, TooltipComponent } from 'echarts/components'
import { CanvasRenderer } from 'echarts/renderers'
import { nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue'

echarts.use([
  BarChart,
  LineChart,
  PieChart,
  RadarChart,
  ScatterChart,
  GridComponent,
  LegendComponent,
  RadarComponent,
  TooltipComponent,
  CanvasRenderer,
])

const props = defineProps({
  option: { type: Object, required: true },
  empty: { type: Boolean, default: false },
  emptyMessage: { type: String, default: '暂无可信分析数据' },
})

const host = ref(null)
let chart
let observer

// 初始化或更新唯一 ECharts 实例；无 option 时清空，避免残留上一组数据。
function render() {
  if (!chart) return
  if (props.empty) { chart.clear(); return }
  chart.setOption(props.option, { notMerge: true, lazyUpdate: true })
}

onMounted(async () => {
  await nextTick()
  chart = echarts.init(host.value, null, { renderer: 'canvas' })
  observer = new ResizeObserver(() => chart?.resize())
  observer.observe(host.value)
  render()
})

watch(() => props.option, render)
watch(() => props.empty, async () => {
  await nextTick()
  render()
  chart?.resize()
})

onBeforeUnmount(() => {
  observer?.disconnect()
  chart?.dispose()
})
</script>

<template>
  <div class="chart-host">
    <div ref="host" class="chart-canvas" :aria-hidden="empty"></div>
    <p v-if="empty" class="empty-state">{{ emptyMessage }}</p>
  </div>
</template>
