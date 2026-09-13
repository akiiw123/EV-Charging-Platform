/* 全国灯点地图:昼夜两套颜色令牌 + 站点呼吸灯 + 站内电流示意连线。
   数据优先取 /api/map(本平台数据库);接口不可用时回退到页面内集中管理的
   DEMO_STATIONS,并在左下角"数据来源"明确标注,不冒充真实业务数据。 */

'use strict';

/* ---------- 可调配置 ---------- */
const DATA_URL = '/api/map';        // 数据服务就绪后改这里即可(如队友 Flask 的完整地址)
const DATA_REFRESH_MS = 30000;      // 数据静默刷新间隔
const THEME_CHECK_MS = 30000;       // 自动模式下复查系统时间的间隔

/* ---------- 昼夜颜色令牌(对应设计稿 §5.1 + 低饱和纸感方案,换肤只改这里) ---------- */
const THEMES = {
  night: { bg:'#04060a', land:'#0e1116', border:'#2c3238', text:'#f4f2ea', muted:'#8b9299',
           glow:'#e6be73', spark:'#fff2ca', idle:'#938777', fault:'#ed7368', offline:'#626b6b',
           panel:'rgba(13,16,20,.72)', panelStrong:'rgba(12,14,17,.92)',
           tipBg:'rgba(17,19,21,.94)',
           kpi:'#f0a45c',
           hairline:'rgba(244,242,234,.10)',
           shadow:'0 10px 28px rgba(0,0,0,.45), 0 0 0 0.5px rgba(244,242,234,.10)',
           areaTop:'rgba(230,190,115,.55)', areaBottom:'rgba(230,190,115,.06)',
           barTop:'rgba(230,190,115,.95)', barBottom:'rgba(230,190,115,.40)' },
  day:   { bg:'#f2f4f6', land:'#e7ecf1', border:'#c3ced9', text:'#26333f', muted:'#65788a',
           glow:'#3e88b0', spark:'#1f6fa8', idle:'#7fb3cd', fault:'#c0564a', offline:'#9aa8b4',
           panel:'rgba(255,255,255,.78)', panelStrong:'rgba(255,255,255,.9)',
           tipBg:'rgba(255,255,255,.97)',
           kpi:'#e2593b',
           hairline:'rgba(38,51,63,.12)',
           shadow:'0 1px 2px rgba(23,32,44,.06), 0 10px 24px rgba(23,32,44,.10), 0 0 0 0.5px rgba(38,51,63,.08)',
           areaTop:'rgba(46,111,159,.85)', areaBottom:'rgba(46,111,159,.05)',
           barTop:'#2e6f9f', barBottom:'#a9cfe6' }
};

const STATUS_LABEL = { charging:'充电中', idle:'空闲', fault:'故障', offline:'离线' };
const STATUS_COLOR_KEY = { charging:'spark', idle:'idle', fault:'fault', offline:'offline' };

/* ---------- 演示数据(集中管理):覆盖各省的示意站点,充电中占多数以呈现灯网效果 ---------- */
function demoStation(name, lng, lat, statuses) {
  return {
    id: 'demo-' + name, name: name, province: '', city: '', demo: true,
    longitude: lng, latitude: lat,
    piles: statuses.map((status, i) => ({
      id: 'demo-' + name + '-' + i, code: 'DP-' + (i + 1),
      power_kw: status === 'charging' ? 60 : 7, status: status, demo: true
    }))
  };
}

const DEMO_STATIONS = [
  demoStation('哈尔滨冰雪站', 126.63, 45.75, ['charging', 'charging', 'idle']),
  demoStation('长春汽开站', 125.32, 43.90, ['charging', 'idle']),
  demoStation('沈阳浑南站', 123.43, 41.80, ['charging', 'fault', 'idle']),
  demoStation('大连东港站', 121.61, 38.91, ['charging', 'idle']),
  demoStation('乌鲁木齐高铁站', 87.62, 43.79, ['charging', 'idle']),
  demoStation('西宁海湖站', 101.78, 36.62, ['idle', 'offline']),
  demoStation('兰州金城站', 103.83, 36.06, ['charging', 'charging', 'idle']),
  demoStation('银川阅海湾站', 106.23, 38.49, ['charging', 'idle']),
  demoStation('呼和浩特青城站', 111.75, 40.84, ['idle', 'idle']),
  demoStation('西安曲江站', 108.94, 34.34, ['charging', 'charging', 'charging', 'idle']),
  demoStation('郑州郑东站', 113.63, 34.75, ['charging', 'idle']),
  demoStation('济南泉城站', 117.12, 36.65, ['charging', 'charging', 'fault']),
  demoStation('青岛湾畔站', 120.38, 36.07, ['charging', 'idle']),
  demoStation('太原汾河站', 112.55, 37.87, ['idle', 'idle']),
  demoStation('成都天府站', 104.07, 30.67, ['charging', 'charging', 'charging', 'idle']),
  demoStation('重庆两江站', 106.55, 29.56, ['charging', 'charging', 'idle']),
  demoStation('昆明滇池站', 102.83, 24.88, ['charging', 'idle']),
  demoStation('贵阳观山湖站', 106.63, 26.65, ['charging', 'fault']),
  demoStation('武汉光谷站', 114.31, 30.59, ['charging', 'charging', 'charging', 'charging']),
  demoStation('长沙湘江站', 112.94, 28.23, ['charging', 'charging', 'idle']),
  demoStation('南昌红谷滩站', 115.89, 28.68, ['charging', 'idle']),
  demoStation('南京河西站', 118.78, 32.06, ['charging', 'charging', 'idle']),
  demoStation('上海陆家嘴站', 121.47, 31.23, ['charging', 'charging', 'charging', 'idle']),
  demoStation('杭州钱江站', 120.16, 30.29, ['charging', 'charging', 'fault']),
  demoStation('福州鼓楼站', 119.30, 26.08, ['charging', 'idle']),
  demoStation('厦门环岛站', 118.09, 24.48, ['charging', 'charging', 'idle']),
  demoStation('广州珠江新城站', 113.26, 23.13, ['charging', 'charging', 'charging', 'idle']),
  demoStation('东莞松山湖站', 113.75, 23.02, ['charging', 'idle']),
  demoStation('南宁五象站', 108.37, 22.82, ['charging', 'idle']),
  demoStation('海口滨海站', 110.32, 20.03, ['charging', 'charging', 'idle']),
  demoStation('拉萨柳梧站', 91.11, 29.66, ['idle', 'offline'])
];

/* ---------- 运行状态 ---------- */
const state = {
  mode: localStorage.getItem('chargingMapMode') || 'auto',   // auto | day | night
  showDemo: localStorage.getItem('chargingMapDemo') !== '0', // 演示铺点默认开启
  source: 'loading',      // loading | database | stale | demo
  generatedAt: null,
  stations: [],           // 真实站点(/api/map)
  camera: { zoom: 1.15, center: null }
};

const chart = echarts.init(document.getElementById('map'));
let mapRegistered = false;
let fxPoints = [];   // 星辉效果层点位缓存,由 buildOption 重建
let fxOrbits = [];   // 电子轨道缓存(每站一条),由 buildOption 重建,fx 层绘制

/* ---------- 工具函数 ---------- */
function themeByHour() {
  const hour = new Date().getHours();
  return (hour >= 6 && hour < 18) ? THEMES.day : THEMES.night;   // 06:00–18:00 日间
}

function effectiveTheme() {
  if (state.mode === 'auto') return themeByHour();
  return state.mode === 'day' ? THEMES.day : THEMES.night;
}

function pileCoord(station, index, total, radius) {
  const angle = (2 * Math.PI * index) / total - Math.PI / 2;
  return [
    station.longitude + radius * Math.cos(angle),
    station.latitude + radius * Math.sin(angle) * 0.8
  ];
}

function chargingCount(station) {
  return station.piles.filter(pile => pile.status === 'charging').length;
}

function normalizeStation(raw) {
  return {
    id: raw.id,
    name: String(raw.name || ('站点 #' + raw.id)),
    province: String(raw.province || ''),
    city: String(raw.city || ''),
    longitude: Number(raw.longitude),
    latitude: Number(raw.latitude),
    demo: false,
    piles: (Array.isArray(raw.piles) ? raw.piles : []).map(pile => ({
      id: pile.id,
      code: String(pile.code || pile.name || '桩'),
      power_kw: Number(pile.power_kw) || 0,
      status: STATUS_LABEL[pile.status] ? pile.status : 'idle'
    }))
  };
}

/* 近邻站点(间距 < minSep 度)按簇错位:簇内成员均匀摆到质心周围的小圆上,
   保证彼此间距 ≥ minSep,让每座站拥有自己的轨道环;只影响绘制,不影响数据 */
function spreadNearby(stations, minSep = 0.06) {
  const parent = stations.map((_, i) => i);
  const find = i => (parent[i] === i ? i : (parent[i] = find(parent[i])));
  for (let i = 0; i < stations.length; i++) {
    for (let j = i + 1; j < stations.length; j++) {
      const d = Math.hypot(stations[i].longitude - stations[j].longitude,
                           stations[i].latitude - stations[j].latitude);
      if (d < minSep) parent[find(i)] = find(j);
    }
  }
  const clusters = {};
  stations.forEach((s, i) => (clusters[find(i)] = clusters[find(i)] || []).push(s));
  Object.values(clusters).forEach(group => {
    if (group.length < 2) return;
    const cx = group.reduce((sum, s) => sum + s.longitude, 0) / group.length;
    const cy = group.reduce((sum, s) => sum + s.latitude, 0) / group.length;
    // 圆半径按成员数收紧:相邻成员间距恰好 ≥ minSep
    const r = Math.max(0.03, minSep / (2 * Math.sin(Math.PI / group.length)));
    group.forEach((s, i) => {
      const a = (2 * Math.PI * i) / group.length - Math.PI / 2;
      s.longitude = cx + r * Math.cos(a);
      s.latitude = cy + r * Math.sin(a);
    });
  });
  return stations;
}

/* ---------- 数据加载 ---------- */
async function loadData() {
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 6000);
    const response = await fetch(DATA_URL, { signal: controller.signal });
    clearTimeout(timer);
    if (!response.ok) throw new Error('HTTP ' + response.status);
    const payload = await response.json();
    if (!Array.isArray(payload.stations)) throw new Error('数据结构不符');
    state.stations = spreadNearby(payload.stations.map(normalizeStation));
    state.source = 'database';
  } catch (error) {
    // 首次失败且从无数据 → 纯演示模式;已有数据 → 保留上次数据并标注接口暂不可用
    state.source = state.stations.length ? 'stale' : 'demo';
  }
  state.generatedAt = new Date();
  updateSourceTag();
  render();
}

/* ---------- 组装 option ---------- */
function stationTooltip(params) {
  const d = params.data;
  return [
    '<b>' + params.name + (d.demo ? '(演示站点·示意)' : '') + '</b>',
    d.region,
    '充电中 ' + d.charging + ' / ' + d.total + ' 桩',
    d.faults ? '故障 ' + d.faults + ' 桩' : ''
  ].filter(Boolean).join('<br>');
}

function pileTooltip(params) {
  const d = params.data;
  return '<b>' + d.code + '</b><br>' + STATUS_LABEL[d.status]
    + (d.demo ? '<br>演示铺点(示意)' : '');
}

function buildOption(theme) {
  const allStations = state.showDemo ? state.stations.concat(DEMO_STATIONS) : state.stations;
  const pilePoints = [], demoRings = [];
  const realGroups = [[], []];   // 真实站点按 id 奇偶分两组呼吸,错开涟漪节奏避免齐闪
  fxPoints = [];                 // 星辉层点位缓存(经纬度 + 稳定相位)
  fxOrbits = [];                 // 电子轨道缓存:每站一条,由 fx 层用 Canvas 绘制

  // 轨道半径自适应:取与最近邻站距离的 45%(0.025~0.12°),紧邻站环线相切不穿插,
  // 独立站保持大环;配合 spreadNearby 的簇内错位,保证"电桩环绕自己的电站"
  const nearestOf = station => {
    let best = Infinity;
    allStations.forEach(other => {
      if (other === station) return;
      const d = Math.hypot(other.longitude - station.longitude,
                           (other.latitude - station.latitude) * 0.8);
      if (d < best) best = d;
    });
    return best;
  };
  allStations.forEach(station => {
    const seed = String(station.id);
    let shash = 0;
    for (let i = 0; i < seed.length; i++) shash = (shash * 31 + seed.charCodeAt(i)) >>> 0;
    const orbitR = Math.min(0.12, Math.max(0.025, nearestOf(station) * 0.45));
    station.piles.forEach((pile, index) => {
      const coord = pileCoord(station, index, station.piles.length, orbitR);
      pilePoints.push({
        value: coord, code: pile.code, status: pile.status, demo: pile.demo,
        symbolSize: pile.power_kw >= 40 ? 8 : (pile.power_kw > 0 ? 6 : 5),
        itemStyle: { color: theme[STATUS_COLOR_KEY[pile.status]] }
      });
      if (pile.status !== 'fault' && pile.status !== 'offline') {
        const seed = String(pile.id || (station.id + '-' + index));
        let hash = 0;
        for (let i = 0; i < seed.length; i++) hash = (hash * 31 + seed.charCodeAt(i)) >>> 0;
        fxPoints.push({
          coord: coord,
          size: pile.power_kw >= 40 ? 3.0 : 2.1,
          phase: (hash % 628) / 100,                 // 稳定相位:重建后不齐跳
          speed: 0.35 + ((hash >> 8) % 50) / 100     // 0.35~0.85 Hz 慢闪
        });
      }
    });
    fxOrbits.push({
      center: [station.longitude, station.latitude], R: orbitR,
      charging: chargingCount(station),
      phase: (shash % 628) / 100,
      speed: 0.25 + (shash % 30) / 100,        // 电子公转角速度 rad/s
      dashSpeed: 8 + shash % 10,               // 轨道虚线旋转 px/s
      hash: shash,
      sparkNext: 900 + shash % 3200, sparkT0: 0, sparkDur: 0, sparkPos: 0, sparkArc: 0
    });
    if (station.demo) {
      demoRings.push({
        name: station.name, demo: true,
        value: [station.longitude, station.latitude]
      });
    } else {
      const charging = chargingCount(station);
      realGroups[Number(station.id) % 2].push({
        name: station.name, demo: false,
        region: [station.province, station.city].filter(Boolean).join(' · '),
        value: [station.longitude, station.latitude, charging],
        charging: charging, total: station.piles.length,
        faults: station.piles.filter(pile => pile.status === 'fault').length
      });
    }
  });

  const stationSeries = realGroups.map((data, index) => ({
    type: 'effectScatter', coordinateSystem: 'geo', data: data, zlevel: 2, z: 5,
    symbolSize: value => 8 + 2 * Math.sqrt(value[2]),
    rippleEffect: { brushType: 'stroke', scale: 2.6, period: index === 0 ? 3.2 : 4.3 },
    itemStyle: { color: theme.glow, shadowBlur: 14, shadowColor: theme.glow },
    tooltip: { formatter: stationTooltip }
  }));

  // 合并大屏传入安全区,让地图落在两侧浮层之间;纯地图页不设置则保持默认铺满布局。
  // 注意:仅在设置了安全区时才添加这些键,显式传 undefined 也会改变 ECharts 默认布局
  const geoLayout = window.MAP_GEO_INSETS ? {
    left: window.MAP_GEO_INSETS.left, right: window.MAP_GEO_INSETS.right,
    top: window.MAP_GEO_INSETS.top, bottom: window.MAP_GEO_INSETS.bottom
  } : {};
  return {
    backgroundColor: 'transparent',   // 透出 body 背景,昼夜切换时 CSS 过渡更平滑
    tooltip: {
      trigger: 'item', confine: true,
      backgroundColor: theme.tipBg, borderColor: theme.border, borderWidth: 1,
      textStyle: { color: theme.text, fontSize: 12 }, extraCssText: 'box-shadow:none;'
    },
    geo: {
      map: 'china', roam: true,
      zoom: state.camera.zoom,
      center: state.camera.center,
      ...geoLayout,
      itemStyle: { areaColor: theme.land, borderColor: theme.border, borderWidth: 0.6 },
      emphasis: { itemStyle: { areaColor: theme.border }, label: { show: false } },
      select: { itemStyle: { areaColor: theme.land }, label: { show: false } }
    },
    series: [
      /* 全部系列统一放在 zlevel 2 的独立叠加层(geo 底图在 zlevel 0)。
         站内连线已改由 fx Canvas 层绘制(曲线 + 流光 + 火花),不再用 lines 系列 */
      { type: 'scatter', coordinateSystem: 'geo', data: pilePoints, zlevel: 2, z: 4,
        symbolSize: 6, emphasis: { scale: 1.6 },
        itemStyle: { borderColor: theme.bg, borderWidth: 1 },
        tooltip: { formatter: pileTooltip } },
      ...stationSeries,
      { type: 'scatter', coordinateSystem: 'geo', data: demoRings, zlevel: 2, z: 6,
        symbolSize: 11,
        itemStyle: { color: 'transparent', borderColor: theme.glow, borderWidth: 1.5 },
        tooltip: { formatter: params => '<b>' + params.name + '</b><br>演示站点(示意,非本平台数据)' } }
    ]
  };
}

/* ---------- 渲染与换肤 ---------- */
function applyCssTokens(theme) {
  const root = document.documentElement.style;
  root.setProperty('--bg', theme.bg);
  root.setProperty('--text', theme.text);
  root.setProperty('--muted', theme.muted);
  root.setProperty('--border', theme.border);
  root.setProperty('--glow', theme.glow);
  root.setProperty('--panel', theme.panel);
  root.setProperty('--panel-strong', theme.panelStrong);
  root.setProperty('--kpi', theme.kpi);
  root.setProperty('--hairline', theme.hairline);
  root.setProperty('--shadow', theme.shadow);
  document.querySelectorAll('.legend [data-c]').forEach(el => {
    el.style.background = theme[el.dataset.c];
  });
}

let lastTheme = null;
function render() {
  const theme = effectiveTheme();
  applyCssTokens(theme);
  if (theme !== lastTheme) {
    lastTheme = theme;
    window.chargingMapTheme = theme;   // 供后加载的页面脚本读取初始主题
    document.dispatchEvent(new CustomEvent('chargingmap:theme', { detail: theme }));
  }
  if (!mapRegistered) return;
  // notMerge 完整重建:缩放等相机变化会让 lines 流光动画器留下跨帧幽灵轨迹,
  // 实测(ECharts 5.6.0)只有重建系列才能确保清除;数据规模下重建成本可忽略
  chart.setOption(buildOption(theme), true);
}

function updateSourceTag() {
  const tag = document.getElementById('sourceTag');
  const pileTotal = state.stations.reduce((total, station) => total + station.piles.length, 0);
  const time = state.generatedAt
    ? state.generatedAt.toLocaleTimeString('zh-CN', { hour12: false }) : '';
  let text;
  if (state.source === 'database') {
    text = '数据来源:本平台数据库 · ' + state.stations.length + ' 站 ' + pileTotal + ' 桩 · 更新于 ' + time;
  } else if (state.source === 'stale') {
    text = '数据来源:本平台数据库(上次更新 ' + time + ',接口暂不可用,显示上次数据)';
  } else if (state.source === 'demo') {
    text = '数据来源:演示数据(/api/map 接口不可用),非真实业务数据';
  } else {
    text = '正在加载数据…';
  }
  if (state.showDemo && state.source !== 'demo') text += ' · 已叠加演示铺点';
  tag.textContent = text;
  tag.classList.toggle('demo', state.source === 'demo');
}

/* ---------- 事件绑定 ---------- */
function syncModeButtons() {
  document.querySelectorAll('.modes button').forEach(button => {
    button.classList.toggle('on', button.dataset.m === state.mode);
  });
}

document.querySelectorAll('.modes button').forEach(button => {
  button.addEventListener('click', () => {
    state.mode = button.dataset.m;
    localStorage.setItem('chargingMapMode', state.mode);
    syncModeButtons();
    render();
  });
});

const demoToggle = document.getElementById('demoToggle');
demoToggle.checked = state.showDemo;
demoToggle.addEventListener('change', () => {
  state.showDemo = demoToggle.checked;
  localStorage.setItem('chargingMapDemo', state.showDemo ? '1' : '0');
  updateSourceTag();
  render();
});

/* 平移/缩放后记录相机状态;手势结束后防抖重建一次,清除流光动画的幽灵轨迹 */
let roamTimer = null;
chart.on('georoam', () => {
  const geo = chart.getOption().geo && chart.getOption().geo[0];
  if (!geo) return;
  state.camera = { zoom: geo.zoom || 1, center: geo.center || null };
  clearTimeout(roamTimer);
  roamTimer = setTimeout(render, 250);
});

/* 缩放按钮:沿当前相机状态倍增/倍减,重置回全国视野 */
document.querySelectorAll('.zoomer button').forEach(button => {
  button.addEventListener('click', () => {
    const action = button.dataset.z;
    if (action === 'reset') {
      state.camera = { zoom: 1.15, center: null };
    } else {
      const factor = action === 'in' ? 1.5 : 1 / 1.5;
      state.camera.zoom = Math.min(40, Math.max(1, state.camera.zoom * factor));
    }
    render();
  });
});

window.addEventListener('resize', () => { chart.resize(); fxResize(); });

function tickClock() {
  document.getElementById('clock').textContent =
    '系统时间 ' + new Date().toLocaleTimeString('zh-CN', { hour12: false });
}
setInterval(tickClock, 1000);
tickClock();

/* ---------- 电子轨道 + 夜间星辉效果层 ----------
   电桩环绕电站排布在虚线轨道环上(fx 层绘制旋转虚线轨道),能量点沿轨道公转
   (数量 = 充电中桩数 + 1),环上随机闪过火花弧段模拟电流滋滋声;
   夜间额外绘制桩点星辉光晕;相邻多站半径错档,呈同心电子壳层 */
const fxCanvas = document.getElementById('fx');
const fxCtx = (fxCanvas && fxCanvas.tagName === 'CANVAS') ? fxCanvas.getContext('2d') : null;
const fxSpriteNight = document.createElement('canvas');
const fxSpriteDay = document.createElement('canvas');
fxSpriteNight.width = fxSpriteNight.height = 48;
fxSpriteDay.width = fxSpriteDay.height = 48;
(() => {
  const mk = (canvas, inner, mid) => {
    const ctx = canvas.getContext('2d');
    const g = ctx.createRadialGradient(24, 24, 0, 24, 24, 24);
    g.addColorStop(0, inner);
    g.addColorStop(0.4, mid);
    g.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 48, 48);
  };
  mk(fxSpriteNight, 'rgba(255,236,190,1)', 'rgba(255,236,190,.4)');
  mk(fxSpriteDay, 'rgba(90,170,215,1)', 'rgba(90,170,215,.4)');
})();

function fxResize() {
  if (!fxCtx) return;
  const dpr = Math.min(window.devicePixelRatio || 1, 1.5);
  // 显式按视口设尺寸,不依赖 CSS(inset) 的加载时序
  fxCanvas.width = Math.max(1, window.innerWidth * dpr);
  fxCanvas.height = Math.max(1, window.innerHeight * dpr);
  fxCanvas.style.width = window.innerWidth + 'px';
  fxCanvas.style.height = window.innerHeight + 'px';
}

function fxFrame(ts) {
  requestAnimationFrame(fxFrame);
  if (!fxCtx || !mapRegistered) return;
  fxCtx.setTransform(1, 0, 0, 1, 0, 0);
  fxCtx.clearRect(0, 0, fxCanvas.width, fxCanvas.height);
  if (document.hidden || (!fxOrbits.length && !fxPoints.length)) return;

  const dpr = Math.min(window.devicePixelRatio || 1, 1.5);
  fxCtx.scale(dpr, dpr);
  const w = fxCanvas.width / dpr, h = fxCanvas.height / dpr;
  const theme = effectiveTheme();
  const night = theme === THEMES.night;
  const t = ts / 1000;

  // ---- 站内电子轨道:旋转虚线环 + 公转能量点 + 环上随机火花 ----
  for (let i = 0; i < fxOrbits.length; i++) {
    const ob = fxOrbits[i];
    let c, px, py;
    try {
      c = chart.convertToPixel({ geoIndex: 0 }, ob.center);
      px = chart.convertToPixel({ geoIndex: 0 }, [ob.center[0] + ob.R, ob.center[1]]);
      py = chart.convertToPixel({ geoIndex: 0 }, [ob.center[0], ob.center[1] + ob.R * 0.8]);
    } catch (error) { return; }
    if (!c || !px || !py || c[0] === undefined || px[0] === undefined || py[0] === undefined) continue;
    const rx = Math.hypot(px[0] - c[0], px[1] - c[1]);
    const ry = Math.hypot(py[0] - c[0], py[1] - c[1]);
    if (rx <= 0 && ry <= 0) continue;
    if (c[0] + Math.max(rx, ry) < -40 || c[0] - Math.max(rx, ry) > w + 40 ||
        c[1] + Math.max(rx, ry) < -40 || c[1] - Math.max(rx, ry) > h + 40) continue;

    // 轨道:虚线环缓慢旋转
    fxCtx.lineWidth = 1;
    fxCtx.strokeStyle = night ? 'rgba(230,190,115,.32)' : 'rgba(46,111,159,.38)';
    fxCtx.setLineDash([3, 7]);
    fxCtx.lineDashOffset = -((t * ob.dashSpeed) % 10);
    fxCtx.beginPath();
    fxCtx.ellipse(c[0], c[1], rx, ry, 0, 0, 6.2832);
    fxCtx.stroke();
    fxCtx.setLineDash([]);

    // 电子能量点:数量 = 充电中桩数 + 1,沿轨道公转
    const dots = Math.min(1 + ob.charging, 4);
    const sprite = night ? fxSpriteNight : fxSpriteDay;
    for (let d = 0; d < dots; d++) {
      const ang = t * ob.speed + (6.2832 * d) / dots + ob.phase;
      const ex = c[0] + rx * Math.cos(ang);
      const ey = c[1] + ry * Math.sin(ang);
      fxCtx.globalAlpha = night ? 0.8 : 0.65;
      fxCtx.drawImage(sprite, ex - 5, ey - 5, 10, 10);
      fxCtx.globalAlpha = 1;
    }

    // 环上随机火花:一小段弧亮起即熄,模拟"滋滋滋"的电流声
    if (ts >= ob.sparkNext) {
      ob.sparkT0 = ts;
      ob.sparkDur = 130 + ob.hash % 150;
      ob.sparkPos = (ob.hash % 628) / 100;
      ob.sparkArc = 0.25 + (ob.hash % 20) / 100;
      ob.sparkNext = ts + 1500 + ob.hash % 3200;
    }
    if (ob.sparkT0 && ts < ob.sparkT0 + ob.sparkDur) {
      const k = (ts - ob.sparkT0) / ob.sparkDur;
      fxCtx.lineWidth = 2;
      fxCtx.strokeStyle = night ? `rgba(255,246,220,${0.85 * Math.sin(Math.PI * k)})`
                                : `rgba(226,89,59,${0.8 * Math.sin(Math.PI * k)})`;
      fxCtx.beginPath();
      fxCtx.ellipse(c[0], c[1], rx, ry, 0, ob.sparkPos, ob.sparkPos + ob.sparkArc);
      fxCtx.stroke();
    }
  }

  // ---- 夜间星辉:桩点光晕(白天不画) ----
  if (night) {
    for (let i = 0; i < fxPoints.length; i++) {
      let p;
      try { p = chart.convertToPixel({ geoIndex: 0 }, fxPoints[i].coord); }
      catch (error) { break; }
      if (!p || p[0] === undefined) continue;
      if (p[0] < -24 || p[0] > w + 24 || p[1] < -24 || p[1] > h + 24) continue;
      const fp = fxPoints[i];
      const wave = 0.5 + 0.5 * Math.sin(t * fp.speed * 2 * Math.PI + fp.phase);
      const alpha = 0.08 + 0.30 * wave;          // 亮度上限 0.38,保持安静
      const r = fp.size * (2.6 + 1.4 * wave);
      fxCtx.globalAlpha = alpha;
      fxCtx.drawImage(fxSpriteNight, p[0] - r, p[1] - r, r * 2, r * 2);
    }
    fxCtx.globalAlpha = 1;
  }
}

if (fxCtx) {
  fxResize();
  requestAnimationFrame(fxFrame);
}

/* 自动模式定期复查系统时间;从后台切回时立即复查,处理改时/休眠跨时段 */
setInterval(render, THEME_CHECK_MS);
document.addEventListener('visibilitychange', () => { if (!document.hidden) render(); });

/* ---------- 启动 ---------- */
syncModeButtons();
render();   // 地图资源就绪前先应用主题令牌并广播,避免白天先亮后黑闪屏
fetch('./assets/china.json')
  .then(response => { if (!response.ok) throw new Error('HTTP ' + response.status); return response.json(); })
  .then(geoJson => {
    echarts.registerMap('china', geoJson);
    mapRegistered = true;
    render();
    loadData();
    setInterval(loadData, DATA_REFRESH_MS);
  })
  .catch(error => {
    document.getElementById('sourceTag').textContent =
      '地图边界加载失败(' + error.message + '):检查 web/dashboard/assets/china.json';
  });
