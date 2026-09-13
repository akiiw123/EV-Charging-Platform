/* 运营数据面板:读取 /api/dashboard(5 秒刷新),图表配色跟随地图昼夜主题。
   map.js 在主题变化时广播 'chargingmap:theme' 并更新 window.chargingMapTheme;
   本文件数据更新或主题切换时用当前主题令牌重绘图表。 */

const revenue = echarts.init(document.getElementById('revenue'));
const statusChart = echarts.init(document.getElementById('status'));
const hourlyChart = echarts.init(document.getElementById('hourly'));
let trendRange = 7;   // 趋势区间:7 / 30 日,服务端固定返回 30 天,前端截取
let theme = window.chargingMapTheme || null;
let lastData = null;

// 主题广播前的兜底令牌(与 map.js 夜间主题一致)
const FALLBACK = { glow:'#e6be73', idle:'#938777', fault:'#ed7368', offline:'#626b6b',
                   text:'#f4f2ea', muted:'#8b9299', border:'#303638',
                   tipBg:'rgba(17,19,21,.94)' };

const STATUS_NAMES = { idle:'空闲', charging:'充电中', fault:'故障', offline:'离线' };
const PANEL_STATUS_COLOR_KEY = { charging:'glow', idle:'idle', fault:'fault', offline:'offline' };

function tooltipTheme(t) {
  return { backgroundColor:t.tipBg, borderColor:t.border, borderWidth:1,
           textStyle:{ color:t.text, fontSize:12 }, extraCssText:'box-shadow:none;' };
}

function renderDashboard(data) {
  const t = theme || FALLBACK;
  document.getElementById('todayRevenue').textContent = `¥ ${Number(data.metrics.today_revenue).toFixed(2)}`;
  document.getElementById('onlinePiles').textContent = data.metrics.online_piles;
  document.getElementById('activeOrders').textContent = data.metrics.active_orders;
  document.getElementById('utilization').textContent = `${Number(data.metrics.utilization || 0).toFixed(1)}%`;

  // 站点营收排行
  const ranking = document.getElementById('ranking');
  ranking.innerHTML = (data.station_ranking || []).map((row, i) =>
    `<li><span class="rank">${i + 1}</span>${row.name}<small>${row.orders} 单 · ¥${Number(row.revenue).toFixed(2)}</small></li>`
  ).join('') || '<li class="empty">暂无数据</li>';

  // 近 7 日充电时段分布:24 小时柱状
  hourlyChart.setOption({
    tooltip: { trigger:'axis', ...tooltipTheme(t) },
    grid: { left:6, right:14, top:14, bottom:4, containLabel:true },
    xAxis: { type:'category',
             data:[...Array(24).keys()].map(h => `${String(h).padStart(2, '0')}:00`),
             axisLabel:{ color:t.muted, fontSize:10, interval:3, hideOverlap:true },
             axisLine:{ lineStyle:{ color:t.border } } },
    yAxis: { type:'value', splitNumber:3, axisLabel:{ color:t.muted, fontSize:10 },
             splitLine:{ lineStyle:{ color:t.border } } },
    series: [{ type:'bar', data:data.hourly_orders,
               itemStyle:{ color:new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                 { offset:0, color:t.barTop }, { offset:1, color:t.barBottom } ]),
                 borderRadius:[3, 3, 0, 0] } }]
  }, true);

  // 营收趋势
  const trend = data.revenue_trend.slice(-trendRange);
  revenue.setOption({
    tooltip: { trigger:'axis', ...tooltipTheme(t) },
    grid: { left:6, right:18, top:14, bottom:4, containLabel:true },
    xAxis: { type:'category', data:trend.map(x => x.date),
             axisLabel:{ color:t.muted, fontSize:10, hideOverlap:true,
                         formatter: value => (value || '').slice(5) },
             axisLine:{ lineStyle:{ color:t.border } } },
    yAxis: { type:'value', splitNumber:3, axisLabel:{ color:t.muted, fontSize:10 },
             splitLine:{ lineStyle:{ color:t.border } } },
    series: [{ type:'line', smooth:true, data:trend.map(x => x.amount),
               itemStyle:{ color:t.glow }, lineStyle:{ color:t.glow },
               symbolSize:5,
               areaStyle:{ color:new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                 { offset:0, color:t.areaTop }, { offset:1, color:t.areaBottom } ]) } }]
  }, true);

  // 电桩状态构成:状态不只靠颜色,悬停与图例都有文字
  statusChart.setOption({
    tooltip: { trigger:'item', ...tooltipTheme(t) },
    legend: { bottom:0, textStyle:{ color:t.muted, fontSize:11 },
              itemWidth:10, itemHeight:10 },
    series: [{ type:'pie', radius:['34%', '54%'], center:['50%', '44%'],
               label:{ show:false },
               data:Object.entries(data.pile_status).map(([key, value]) => ({
                 name:STATUS_NAMES[key] || key, value,
                 itemStyle:{ color:t[PANEL_STATUS_COLOR_KEY[key]] || t.idle } })) }]
  }, true);
}

async function refresh() {
  const state = document.getElementById('connectionState');
  try {
    const response = await fetch('/api/dashboard', { cache:'no-store' });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    lastData = await response.json();
    renderDashboard(lastData);
    state.textContent = `数据已更新：${new Date().toLocaleTimeString('zh-CN', { hour12:false })}`;
  } catch (error) { state.textContent = `数据读取失败：${error.message}`; }
}

// 地图昼夜切换 → 立即用新主题重绘(不重新请求)
document.addEventListener('chargingmap:theme', event => {
  theme = event.detail;
  if (lastData) renderDashboard(lastData);
});

for (const [btn, range] of [['range7', 7], ['range30', 30]]) {
  document.getElementById(btn).onclick = () => {
    trendRange = range;
    document.getElementById('range7').className = range === 7 ? 'on' : '';
    document.getElementById('range30').className = range === 30 ? 'on' : '';
    if (lastData) renderDashboard(lastData);
  };
}

refresh();
setInterval(refresh, 5000);
addEventListener('resize', () => { revenue.resize(); statusChart.resize(); hourlyChart.resize(); });
