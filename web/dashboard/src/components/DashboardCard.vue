<script setup>
import { BorderBox8 } from '@kjgl77/datav-vue3'
import { inject, computed, ref, onMounted, onBeforeUnmount } from 'vue'
const borderBox = ref(null)
let resizeObserver
let resizeFrame = 0

// DataV observes inline style mutations, not sizes changed by parent CSS animations.
// Refresh its SVG geometry without remounting the card or its chart contents.
onMounted(() => {
  resizeObserver = new ResizeObserver(() => {
    if (resizeFrame) return
    resizeFrame = requestAnimationFrame(() => {
      resizeFrame = 0
      borderBox.value?.initWH(false)
    })
  })
  if (borderBox.value?.$el) resizeObserver.observe(borderBox.value.$el)
})

onBeforeUnmount(() => {
  resizeObserver?.disconnect()
  cancelAnimationFrame(resizeFrame)
})

const theme = inject('dashboardTheme', 'night')
const borderColors = computed(() => theme.value === 'day' ? ['#319d91', '#b9d3d8'] : ['#69c9c2', '#557a95'])

defineProps({
  title: { type: String, required: true },
  eyebrow: { type: String, default: '' },
  badge: { type: String, default: '' },
})
</script>

<template>
  <BorderBox8 ref="borderBox" class="dashboard-card" :dur="12" :color="borderColors">
    <section class="dashboard-card__inner">
      <header class="dashboard-card__header">
        <div>
          <small>{{ eyebrow }}</small>
          <h2>{{ title }}</h2>
        </div>
        <span v-if="badge" class="dashboard-card__badge">{{ badge }}</span>
      </header>
      <div class="dashboard-card__content"><slot /></div>
    </section>
  </BorderBox8>
</template>
