<script setup>
import { BorderBox8 } from '@kjgl77/datav-vue3'

defineProps({
  title: { type: String, required: true },
  eyebrow: { type: String, default: '' },
  badge: { type: String, default: '' },
})
</script>

<template>
  <BorderBox8 class="dashboard-card" :dur="5" :color="['#1de9c4', '#19476b']">
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
