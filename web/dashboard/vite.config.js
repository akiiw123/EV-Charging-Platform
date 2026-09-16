/**
 * 功能：配置 Vue/Vite 开发服务器、生产路径和分析 API 代理。
 * 输入：ANALYTICS_PROXY_TARGET，默认 http://127.0.0.1:8091。
 * 输出/接口：页面统一部署到 /dashboard/，开发和 preview 时把 /api/v1 转发给 Flask。
 */
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
