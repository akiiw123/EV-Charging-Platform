<!--
  功能：把全国 Canvas 地图包装成 Vue 组件并提供省份、视角、缩放、预测和站点详情交互。
  输入：当前模式站点、统计数据、主题、加载状态和错误信息。
  输出/接口：调用 createMap，并在地图中央提供可收起的规则预测卡片。
-->
<script setup>
import { nextTick, onBeforeUnmount, onMounted, shallowRef, ref, watch } from 'vue'
import DashboardCard from './DashboardCard.vue'
import PredictionAssistant from './PredictionAssistant.vue'
import { createMap } from '../lib/map-view.js'

const props = defineProps({
  stationData: { type: Object, default: null },
  theme: { type: String, default: 'night' },
  loading: { type: Boolean, default: false },
  error: { type: String, default: '' },
  historical: { type: Boolean, default: false },
  predictionData: { type: Object, default: null },
  dataMode: { type: String, default: 'live' },
})

const selectedStation = ref(null)
const stationDialog = ref(null)
const scope = ref({ name: '全国', stations: 0 })
const view = ref('2d')
const layout = ref('immersive')
const links = ref(true)
const motion = ref(true)
const map = shallowRef(null)

function openStation(station) {
  selectedStation.value = station
  stationDialog.value?.showModal()
}

function selectProvince(event) {
  map.value?.selectProvince(event.target.value || null)
}

function returnNational() {
  map.value?.selectProvince(null)
}

function setView(next) {
  view.value = next
  map.value?.setView(next)
}

function setLayout(next) {
  layout.value = next
  map.value?.setLayout(next)
}

function zoomMap(factor) {
  map.value?.zoom(factor)
}

function fitMap() {
  map.value?.fit()
}

onMounted(async () => {
  await nextTick()

  map.value = createMap(null, openStation, {
    onScopeChange: state => {
      scope.value = state
    },
  })

  map.value.setLayout(layout.value)
  map.value.setLinks(links.value)
  map.value.setMotion(motion.value)

  if (props.stationData) {
    map.value.update(props.stationData)
  }
})

watch(
  () => props.stationData,
  value => {
    if (value) map.value?.update(value)
  },
)

watch(
  () => props.theme,
  () => {
    map.value?.theme()
  },
)

onBeforeUnmount(() => {
  map.value?.dispose()
})
</script>

<template>
  <DashboardCard :title="historical ? '历史批次站点分布' : '业务充电站分布'" eyebrow="STATION MAP" :badge="`${scope.stations || 0} 站`" class="map-card">
    <div class="map-panel">
      <div class="map-toolbar">
        <select
          id="provinceSelect"
          aria-label="选择省份"
          @change.stop="selectProvince"
        ></select>
        <button
          id="backNational"
          type="button"
          hidden
          @click.stop="returnNational"
        >
          返回全国
        </button>
        <div class="segmented" aria-label="地图视角">
          <button
            v-for="item in ['2d', '2.5d']"
            :key="item"
            type="button"
            :data-view="item"
            :aria-pressed="view === item"
            @click.stop="setView(item)"
          >
            {{ item.toUpperCase() }}
          </button>
        </div>
        <PredictionAssistant :dashboard="predictionData" :mode="dataMode" />
      </div>

      <div class="map-meta">
        <strong><span id="mapScope">全国</span> / {{ historical ? '历史站点' : '业务站点' }}</strong>
        <span id="mapViewName">2D 俯瞰地图</span>
        <span v-if="historical" class="map-coordinate-note">展示坐标 · 按站点名称近似生成</span>
      </div>

      <div class="map-stage">
        <div id="mapBackdrop"></div>
        <div id="map" role="application" tabindex="0" :aria-label="historical ? '历史批次充电站交互地图' : '业务充电站交互地图'"></div>
        <p id="mapMessage" class="map-message" :hidden="!loading && !error">{{ error || '正在加载站点资料…' }}</p>
        <div class="map-zoom">
            <button type="button" aria-label="放大地图" @click.stop="zoomMap(1.5)">＋</button>
            <button type="button" aria-label="缩小地图" @click.stop="zoomMap(1 / 1.5)">－</button>
            <button type="button" aria-label="复位地图" @click.stop="fitMap"> ⌂</button>
        </div>
      </div>
    </div>
  </DashboardCard>

  <dialog ref="stationDialog" class="station-dialog" @close="selectedStation = null">
    <template v-if="selectedStation">
      <button class="dialog-close" type="button" aria-label="关闭" @click="stationDialog.close()">×</button>
      <small>{{ historical ? 'SPARK HISTORICAL STATION' : 'PLATFORM BUSINESS STATION' }}</small>
      <h2>{{ selectedStation.name }}</h2>
      <p>{{ [selectedStation.province, selectedStation.city].filter(Boolean).join(' / ') || '行政区域未知' }}</p>
      <dl>
        <div><dt>地址</dt><dd>{{ selectedStation.address || '未提供' }}</dd></div>
        <div><dt>充电设备</dt><dd>{{ Object.values(selectedStation.counts).reduce((sum, value) => sum + value, 0) }} 个</dd></div>
        <div v-if="historical"><dt>坐标说明</dt><dd>原数据无经纬度；当前位置按站点名称和地址近似生成，仅供大屏展示</dd></div>
        <div v-else><dt>空闲 / 充电 / 故障 / 离线</dt><dd>{{ selectedStation.counts.idle }} / {{ selectedStation.counts.charging }} / {{ selectedStation.counts.fault }} / {{ selectedStation.counts.offline }}</dd></div>
      </dl>
    </template>
  </dialog>
</template>
