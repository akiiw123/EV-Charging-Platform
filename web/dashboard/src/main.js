/**
 * 功能：Vue3 大屏的浏览器启动入口。
 * 输入：根组件 App.vue 和全局样式 styles.css。
 * 输出/接口：创建 Vue 应用并挂载到 index.html 的 #app。
 */
import { createApp } from 'vue'
import App from './App.vue'
import './styles.css'

createApp(App).mount('#app')
