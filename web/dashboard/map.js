/* 全国灯点地图:昼夜两套颜色令牌 + 站点呼吸灯 + 站内电流示意连线。
   数据优先取 /api/map(本平台数据库);接口不可用时回退到页面内集中管理的
   DEMO_STATIONS,并在左下角"数据来源"明确标注,不冒充真实业务数据。 */

'use strict';

/* ---------- 可调配置 ---------- */
const DATA_URL = '/api/map';        // 数据服务就绪后改这里即可(如队友 Flask 的完整地址)
const DATA_REFRESH_MS = 30000;      // 数据静默刷新间隔
const THEME_CHECK_MS = 30000;       // 自动模式下复查系统时间的间隔
const PILE_RING_RADIUS = 0.12;      // 桩位示意半径(度):仅示意布局,不代表真实相对位置

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
let fxLinks = [];    // 电流连线缓存(站点→桩),由 buildOption 重建,fx 层绘制

/* ---------- 工具函数 ---------- */
function themeByHour() {
  const hour = new Date().getHours();
  return (hour >= 6 && hour < 18) ? THEMES.day : THEMES.night;   // 06:00–18:00 日间
}

function effectiveTheme() {
  if (state.mode === 'auto') return themeByHour();
  return state.mode === 'day' ? THEMES.day : THEMES.night;
}

function pileCoord(station, index, total) {
  const angle = (2 * Math.PI * index) / total - Math.PI / 2;
  return [
    station.longitude + PILE_RING_RADIUS * Math.cos(angle),
    station.latitude + PILE_RING_RADIUS * Math.sin(angle) * 0.8
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

/* 同坐标或极近的多座站,展示时按固定序号微小平移避免完全重叠(只影响绘制,不影响数据) */
function spreadDuplicates(stations) {
  const groups = {};
  stations.forEach(station => {
    const key = station.longitude.toFixed(3) + ',' + station.latitude.toFixed(3);
    (groups[key] = groups[key] || []).push(station);
  });
  Object.keys(groups).forEach(key => {
    const group = groups[key];
    if (group.length < 2) return;
    group.forEach((station, index) => {
      const angle = (2 * Math.PI * index) / group.length;
      station.longitude += 0.06 * Math.cos(angle);
      station.latitude += 0.06 * Math.sin(angle);
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
    state.stations = spreadDuplicates(payload.stations.map(normalizeStation));
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
  fxLinks = [];                  // 电流连线缓存:站点 → 桩,由 fx 层用 Canvas 绘制

  allStations.forEach(station => {
    station.piles.forEach((pile, index) => {
      const coord = pileCoord(station, index, station.piles.length);
      pilePoints.push({
        value: coord, code: pile.code, status: pile.status, demo: pile.demo,
        symbolSize: pile.power_kw >= 40 ? 8 : (pile.power_kw > 0 ? 6 : 5),
        itemStyle: { color: theme[STATUS_COLOR_KEY[pile.status]] }
      });
      const seed = String(pile.id || (station.id + '-' + index));
      let hash = 0;
      for (let i = 0; i < seed.length; i++) hash = (hash * 31 + seed.charCodeAt(i)) >>> 0;
      // 电流连线参数:弯曲方向/弧度/流速/火花节奏均由桩 ID 稳定派生,不齐刷刷
      fxLinks.push({
        from: [station.longitude, station.latitude], to: coord, status: pile.status,
        side: index % 2 ? 1 : -1,
        curv: 0.14 + (hash % 9) / 100,
        speed: 0.5 + (hash % 40) / 100,
        phase: (hash % 628) / 100,
        hash: hash,
        sparkNext: 800 + hash % 3600, sparkT0: 0, sparkDur: 0, sparkPos: 0, sparkLen: 0
      });
      if (pile.status !== 'fault' && pile.status !== 'offline') {
        fxPoints.push({
          coord: coord,
          size: pile.power_kw >= 40 ? 3.0 : 2.1,
          phase: (hash % 628) / 100,                 // 稳定相位:重建后不齐跳
          speed: 0.35 + ((hash >> 8) % 50) / 100     // 0.35~0.85 Hz 慢闪
        });
      }
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

/* ---------- 电流连线 + 夜间星辉效果层 ----------
   站内连接改由 Canvas 绘制:贝塞尔曲线(交替弯向) + 沿线流动短划 +
   随机火花(模拟电流滋滋声),状态语义不变:充电中流动、空闲淡线、故障红虚线。
   夜间额外绘制桩点星辉光晕;白天整体静止、只保留低速流光 */
const fxCanvas = document.getElementById('fx');
const fxCtx = (fxCanvas && fxCanvas.tagName === 'CANVAS') ? fxCanvas.getContext('2d') : null;
const fxSprite = document.createElement('canvas');
fxSprite.width = fxSprite.height = 48;
(() => {
  const ctx = fxSprite.getContext('2d');
  const g = ctx.createRadialGradient(24, 24, 0, 24, 24, 24);
  g.addColorStop(0, 'rgba(255,236,190,1)');
  g.addColorStop(0.4, 'rgba(255,236,190,.4)');
  g.addColorStop(1, 'rgba(255,236,190,0)');
  ctx.fillStyle = g;
  ctx.fillRect(0, 0, 48, 48);
})();

function bez(p0, p1, p2, u) {
  const v = 1 - u;
  return [v * v * p0[0] + 2 * v * u * p1[0] + u * u * p2[0],
          v * v * p0[1] + 2 * v * u * p1[1] + u * u * p2[1]];
}

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
  if (document.hidden || (!fxLinks.length && !fxPoints.length)) return;

  const dpr = Math.min(window.devicePixelRatio || 1, 1.5);
  fxCtx.scale(dpr, dpr);
  const w = fxCanvas.width / dpr, h = fxCanvas.height / dpr;
  const theme = effectiveTheme();
  const night = theme === THEMES.night;
  const t = ts / 1000;

  // ---- 站内电流连接 ----
  for (let i = 0; i < fxLinks.length; i++) {
    const link = fxLinks[i];
    let a, b;
    try {
      a = chart.convertToPixel({ geoIndex: 0 }, link.from);
      b = chart.convertToPixel({ geoIndex: 0 }, link.to);
    } catch (error) { return; }
    if (!a || !b || a[0] === undefined || b[0] === undefined) continue;
    if (Math.max(a[0], b[0]) < -30 || Math.min(a[0], b[0]) > w + 30 ||
        Math.max(a[1], b[1]) < -30 || Math.min(a[1], b[1]) > h + 30) continue;

    const dx = b[0] - a[0], dy = b[1] - a[1];
    const len = Math.hypot(dx, dy) || 1;
    // 控制点:垂直偏移交替弯向 + 轻微摆动,让曲线有“活着”的感觉
    const sway = Math.sin(t * 1.7 + link.phase) * Math.min(2.5, len * 0.015);
    const cxm = (a[0] + b[0]) / 2 - (dy / len) * len * link.curv * link.side + (dx / len) * sway;
    const cym = (a[1] + b[1]) / 2 + (dx / len) * len * link.curv * link.side + (dy / len) * sway;
    const path = () => {
      fxCtx.beginPath();
      fxCtx.moveTo(a[0], a[1]);
      fxCtx.quadraticCurveTo(cxm, cym, b[0], b[1]);
    };

    // 基线:细而淡
    fxCtx.lineWidth = 1;
    fxCtx.strokeStyle = night ? 'rgba(230,190,115,.17)' : 'rgba(46,111,159,.30)';
    path(); fxCtx.stroke();

    if (link.status === 'charging') {
      // 流动短划:能量从站点流向桩
      fxCtx.setLineDash([2.5, 12]);
      fxCtx.lineDashOffset = -((t * link.speed * 30) % 14.5);
      fxCtx.lineWidth = 1.5;
      fxCtx.strokeStyle = night ? 'rgba(255,236,190,.6)' : 'rgba(31,111,168,.55)';
      path(); fxCtx.stroke();
      fxCtx.setLineDash([]);
      // 随机火花:一小段亮光一闪而过,模拟“滋滋滋”的电流声
      if (ts >= link.sparkNext) {
        link.sparkT0 = ts;
        link.sparkDur = 130 + link.hash % 150;
        link.sparkPos = 0.22 + (link.hash % 46) / 100;
        link.sparkLen = 0.10 + (link.hash % 14) / 100;
        link.sparkNext = ts + 1600 + link.hash % 3400;
      }
      if (link.sparkT0 && ts < link.sparkT0 + link.sparkDur) {
        const k = (ts - link.sparkT0) / link.sparkDur;
        const alpha = Math.sin(Math.PI * k);
        const p1 = [cxm, cym];
        const s0 = bez(a, p1, b, link.sparkPos);
        const s1 = bez(a, p1, b, Math.min(0.98, link.sparkPos + link.sparkLen));
        fxCtx.lineWidth = 2.2;
        fxCtx.strokeStyle = night ? `rgba(255,246,220,${0.9 * alpha})`
                                  : `rgba(226,89,59,${0.8 * alpha})`;
        fxCtx.beginPath(); fxCtx.moveTo(s0[0], s0[1]); fxCtx.lineTo(s1[0], s1[1]); fxCtx.stroke();
        fxCtx.fillStyle = night ? `rgba(255,250,235,${0.9 * alpha})`
                                : `rgba(226,89,59,${0.85 * alpha})`;
        fxCtx.beginPath(); fxCtx.arc(s1[0], s1[1], 1.7, 0, 6.2832); fxCtx.fill();
      }
    } else if (link.status === 'fault') {
      fxCtx.setLineDash([3, 4]);
      fxCtx.lineWidth = 1;
      fxCtx.strokeStyle = night ? 'rgba(237,115,104,.5)' : 'rgba(192,86,74,.55)';
      path(); fxCtx.stroke();
      fxCtx.setLineDash([]);
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
      fxCtx.drawImage(fxSprite, p[0] - r, p[1] - r, r * 2, r * 2);
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
