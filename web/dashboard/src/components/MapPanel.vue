<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import DashboardCard from './DashboardCard.vue'
import { createMap } from '../lib/map-view.js'

const props = defineProps({
  stationData: { type: Object, default: null },
  theme: { type: String, default: 'night' },
  loading: { type: Boolean, default: false },
  error: { type: String, default: '' },
})

const selectedStation = ref(null)
const stationDialog = ref(null)
const scope = ref({ name: '全国', stations: 0 })
const view = ref('2d')
const layout = ref('panels')
const links = ref(false)
const motion = ref(false)
let map

const sourceLink = computed(() => {
  const value = selectedStation.value?.sourceUrl || ''
  return /^https:\/\/www\.openstreetmap\.org\/(node|way|relation)\/\d+$/.test(value) ? value : ''
})

function openStation(station) {
  selectedStation.value = station
  stationDialog.value?.showModal()
}

function selectProvince(event) {
  map?.selectProvince(event.target.value || null)
}

function setView(next) {
  view.value = next
  map?.setView(next)
}

function setLayout(next) {
  layout.value = next
  map?.setLayout(next)
}

function toggleLinks() {
  links.value = !links.value
  map?.setLinks(links.value)
}

function toggleMotion() {
  motion.value = !motion.value
  map?.setMotion(motion.value)
}

onMounted(async () => {
  await nextTick()
  map = createMap(null, openStation, { onScopeChange: state => { scope.value = state } })
  map.setLayout(layout.value)
  if (props.stationData) map.update(props.stationData)
})

watch(() => props.stationData, value => { if (value) map?.update(value) })
watch(() => props.theme, () => map?.theme())

onBeforeUnmount(() => map?.dispose())
</script>

<template>
  <DashboardCard title="全国充电网络" eyebrow="GEOGRAPHIC ASSET VIEW" :badge="`${scope.stations || 0} 站`" class="map-card">
    <div class="map-panel">
      <div class="map-toolbar">
        <select id="provinceSelect" aria-label="选择省份" @change="selectProvince"></select>
        <button id="backNational" type="button" hidden @click="map?.selectProvince(null)">返回全国</button>
        <div class="segmented" aria-label="地图视角">
          <button v-for="item in ['2d', '2.5d']" :key="item" type="button" :data-view="item"
                  :aria-pressed="view === item" @click="setView(item)">{{ item.toUpperCase() }}</button>
        </div>
        <button id="toggleLinks" type="button" :aria-pressed="links" @click="toggleLinks">近邻连线</button>
        <button id="toggleMotion" type="button" :aria-pressed="motion" @click="toggleMotion">静态灯光</button>
      </div>

      <div class="map-meta">
        <strong><span id="mapScope">全国</span> / 充电网络</strong>
        <span id="mapViewName">2D 俯瞰地图</span>
      </div>

      <div class="map-stage">
        <div id="mapBackdrop"></div>
        <div id="map" role="application" tabindex="0" aria-label="全国充电站交互地图"></div>
        <p id="mapMessage" class="map-message" :hidden="!loading && !error">{{ error || '正在加载站点资料…' }}</p>
        <div class="map-zoom">
          <button type="button" aria-label="放大地图" @click="map?.zoom(1.5)">＋</button>
          <button type="button" aria-label="缩小地图" @click="map?.zoom(1 / 1.5)">－</button>
          <button type="button" aria-label="复位地图" @click="map?.fit()">⌂</button>
        </div>
      </div>

      <div class="map-footer">
        <span id="lightCount">0 个站点灯光</span>
        <span id="linkCount" hidden>近邻连线已关闭</span>
        <span class="legend-disclaimer" hidden>近邻线仅表示空间接近，不代表电网连接</span>
        <span id="scopeHint">点击省域下钻 · 点击站点查看详情 · 拖动 / 缩放</span>
        <span id="geometryNote">省级底图</span>
        <span id="layoutHint"></span>
        <div class="layout-controls" hidden>
          <button v-for="item in ['panels', 'immersive']" :key="item" type="button" :data-layout="item"
                  :aria-pressed="layout === item" @click="setLayout(item)">{{ item }}</button>
        </div>
      </div>
    </div>
  </DashboardCard>

  <dialog ref="stationDialog" class="station-dialog" @close="selectedStation = null">
    <template v-if="selectedStation">
      <button class="dialog-close" type="button" aria-label="关闭" @click="stationDialog.close()">×</button>
      <small>OSM STATIC STATION</small>
      <h2>{{ selectedStation.name }}</h2>
      <p>{{ [selectedStation.province, selectedStation.city].filter(Boolean).join(' / ') || '行政区域未知' }}</p>
      <dl>
        <div><dt>运营商</dt><dd>{{ selectedStation.operator || '未知' }}</dd></div>
        <div><dt>品牌</dt><dd>{{ selectedStation.brand || '未知' }}</dd></div>
        <div><dt>地址</dt><dd>{{ selectedStation.address || '未提供' }}</dd></div>
        <div><dt>设备状态</dt><dd>静态资料未提供，不能推断</dd></div>
      </dl>
      <a v-if="sourceLink" :href="sourceLink" target="_blank" rel="noopener noreferrer">查看 OpenStreetMap 来源</a>
    </template>
  </dialog>
</template>
