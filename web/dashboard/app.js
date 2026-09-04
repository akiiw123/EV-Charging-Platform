const revenue = echarts.init(document.getElementById('revenue'));
const statusChart = echarts.init(document.getElementById('status'));
const hourlyChart = echarts.init(document.getElementById('hourly'));
let trendRange = 7;   // 趋势区间:7 / 30 日,服务端固定返回 30 天,前端截取

async function refresh() {
  const state = document.getElementById('connectionState');
  try {
    const response = await fetch('/api/dashboard', { cache: 'no-store' });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const data = await response.json();
    document.getElementById('todayRevenue').textContent = `¥ ${Number(data.metrics.today_revenue).toFixed(2)}`;
    document.getElementById('onlinePiles').textContent = data.metrics.online_piles;
    document.getElementById('activeOrders').textContent = data.metrics.active_orders;
    document.getElementById('utilization').textContent = `${Number(data.metrics.utilization || 0).toFixed(1)}%`;
    // 站点营收排行
    const ranking = document.getElementById('ranking');
    ranking.innerHTML = (data.station_ranking || []).map((row, i) =>
      `<li><span class="rank">${i + 1}</span>${row.name}<small>${row.orders} 单 · ¥${Number(row.revenue).toFixed(2)}</small></li>`
    ).join('') || '<li class="empty">暂无数据</li>';
    // 时段分布:24 小时柱状
    hourlyChart.setOption({
      tooltip: {trigger: 'axis'},
      xAxis: {type: 'category', data: [...Array(24).keys()].map(h => `${String(h).padStart(2, '0')}:00`), axisLabel: {color: '#9bbbd8', interval: 3}},
      yAxis: {type: 'value', axisLabel: {color: '#9bbbd8'}, splitLine: {lineStyle: {color: '#173d60'}}},
      series: [{type: 'bar', data: data.hourly_orders, itemStyle: {color: '#55a8f0', borderRadius: [3, 3, 0, 0]}}]
    });
    const trend = data.revenue_trend.slice(-trendRange);
    revenue.setOption({tooltip:{trigger:'axis'},xAxis:{type:'category',data:trend.map(x=>x.date),axisLabel:{color:'#9bbbd8'}},
      yAxis:{type:'value',axisLabel:{color:'#9bbbd8'},splitLine:{lineStyle:{color:'#173d60'}}},
      series:[{type:'line',smooth:true,data:trend.map(x=>x.amount),areaStyle:{},itemStyle:{color:'#55d6be'}}]});
    const names = {idle:'空闲',charging:'充电中',fault:'故障',offline:'离线'};
    statusChart.setOption({tooltip:{trigger:'item'},series:[{type:'pie',radius:['45%','70%'],
      data:Object.entries(data.pile_status).map(([key,value])=>({name:names[key]||key,value}))}]});
    state.textContent = `数据已更新：${new Date().toLocaleTimeString('zh-CN')}`;
  } catch (error) { state.textContent = `数据读取失败：${error.message}`; }
}
function tick(){document.getElementById('clock').textContent=new Date().toLocaleString('zh-CN');}
tick(); refresh(); setInterval(tick,1000); setInterval(refresh,5000);
for (const [btn, range] of [['range7', 7], ['range30', 30]]) {
  document.getElementById(btn).onclick = () => {
    trendRange = range;
    document.getElementById('range7').className = range === 7 ? 'on' : '';
    document.getElementById('range30').className = range === 30 ? 'on' : '';
    refresh();
  };
}
addEventListener('resize',()=>{revenue.resize();statusChart.resize();hourlyChart.resize();});
