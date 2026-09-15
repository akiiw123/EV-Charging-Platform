import { defineConfig, loadEnv } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), '')
  return {
    base: '/dashboard/',
    plugins: [vue()],
    server: {
      host: '0.0.0.0',
      port: 5173,
      strictPort: true,
      proxy: {
        '/api/v1': {
          target: env.ANALYTICS_PROXY_TARGET || 'http://127.0.0.1:8091',
          changeOrigin: true,
        },
      },
    },
    preview: {
      host: '0.0.0.0',
      port: 4173,
      strictPort: true,
      proxy: {
        '/api/v1': { target: env.ANALYTICS_PROXY_TARGET || 'http://127.0.0.1:8091', changeOrigin: true },
      },
    },
  }
})
