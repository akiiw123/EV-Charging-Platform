const revenue = echarts.init(document.getElementById('revenue'));
revenue.setOption({
  tooltip: { trigger: 'axis' },
  xAxis: { type: 'category', data: ['周一','周二','周三','周四','周五','周六','周日'], axisLabel: { color: '#9bbbd8' } },
  yAxis: { type: 'value', axisLabel: { color: '#9bbbd8' }, splitLine: { lineStyle: { color: '#173d60' } } },
  series: [{ type: 'line', smooth: true, data: [0,0,0,0,0,0,0], areaStyle: {}, itemStyle: { color: '#55d6be' } }]
});
const status = echarts.init(document.getElementById('status'));
status.setOption({
  tooltip: { trigger: 'item' },
  series: [{ type: 'pie', radius: ['45%','70%'], data: [
    { value: 0, name: '空闲' }, { value: 0, name: '充电中' }, { value: 0, name: '故障' }
  ] }]
});
function tick() { document.getElementById('clock').textContent = new Date().toLocaleString('zh-CN'); }
tick(); setInterval(tick, 1000);
addEventListener('resize', () => { revenue.resize(); status.resize(); });
