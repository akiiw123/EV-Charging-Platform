const revenue = echarts.init(document.getElementById('revenue'));
const statusChart = echarts.init(document.getElementById('status'));

async function refresh() {
  const state = document.getElementById('connectionState');
  try {
    const response = await fetch('/api/dashboard', { cache: 'no-store' });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const data = await response.json();
    document.getElementById('todayRevenue').textContent = `¥ ${Number(data.metrics.today_revenue).toFixed(2)}`;
    document.getElementById('onlinePiles').textContent = data.metrics.online_piles;
    document.getElementById('activeOrders').textContent = data.metrics.active_orders;
    revenue.setOption({tooltip:{trigger:'axis'},xAxis:{type:'category',data:data.revenue_trend.map(x=>x.date),axisLabel:{color:'#9bbbd8'}},
      yAxis:{type:'value',axisLabel:{color:'#9bbbd8'},splitLine:{lineStyle:{color:'#173d60'}}},
      series:[{type:'line',smooth:true,data:data.revenue_trend.map(x=>x.amount),areaStyle:{},itemStyle:{color:'#55d6be'}}]});
    const names = {idle:'空闲',charging:'充电中',fault:'故障',offline:'离线'};
    statusChart.setOption({tooltip:{trigger:'item'},series:[{type:'pie',radius:['45%','70%'],
      data:Object.entries(data.pile_status).map(([key,value])=>({name:names[key]||key,value}))}]});
    state.textContent = `数据已更新：${new Date().toLocaleTimeString('zh-CN')}`;
  } catch (error) { state.textContent = `数据读取失败：${error.message}`; }
}
function tick(){document.getElementById('clock').textContent=new Date().toLocaleString('zh-CN');}
tick(); refresh(); setInterval(tick,1000); setInterval(refresh,5000);
addEventListener('resize',()=>{revenue.resize();statusChart.resize();});
