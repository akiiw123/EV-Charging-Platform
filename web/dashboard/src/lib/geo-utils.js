/**
 * 功能：提供地图投影、边界、点落省份、近邻连线等纯几何算法。
 * 输入：原始 GeoJSON 和经纬度；不修改输入坐标，也不访问 DOM。
 * 输出/接口：createProjection、prepareRegions、locateProvince、proximityEdges 等可测试函数。
 */
export const clamp = (n, low, high) => Math.max(low, Math.min(high, n));
export function hash(value) {
  let h = 2166136261;
  for (const c of String(value)) { h ^= c.charCodeAt(0); h = Math.imul(h, 16777619); }
  return h >>> 0;
}
export function polygons(feature) {
  const g = feature?.geometry;
  if (g?.type === 'Polygon') return [g.coordinates];
  if (g?.type === 'MultiPolygon') return g.coordinates;
  return [];
}
export function boundsOf(features) {
  const b = [Infinity, Infinity, -Infinity, -Infinity];
  for (const f of features) for (const p of polygons(f)) for (const r of p) for (const [x, y] of r) {
    b[0] = Math.min(b[0], x); b[1] = Math.min(b[1], y); b[2] = Math.max(b[2], x); b[3] = Math.max(b[3], y);
  }
  return b.every(Number.isFinite) ? b : null;
}
export function inBounds([x, y], b) { return b && x >= b[0] && x <= b[2] && y >= b[1] && y <= b[3]; }
function inRing([x, y], ring) {
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const [xi, yi] = ring[i], [xj, yj] = ring[j];
    // Treat points exactly on an edge as contained (stable administrative boundary lookup).
    const cross = (x - xi) * (yj - yi) - (y - yi) * (xj - xi);
    if (Math.abs(cross) < 1e-10 && x >= Math.min(xi, xj) && x <= Math.max(xi, xj) && y >= Math.min(yi, yj) && y <= Math.max(yi, yj)) return true;
    if (((yi > y) !== (yj > y)) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) inside = !inside;
  }
  return inside;
}
export function pointInFeature(point, feature) {
  if (!point || !point.every(Number.isFinite)) return false;
  return polygons(feature).some(poly => inRing(point, poly[0]) && !poly.slice(1).some(ring => inRing(point, ring)));
}
export function provinceName(name) {
  return String(name ?? '').replace(/维吾尔自治区|壮族自治区|回族自治区|特别行政区|自治区|省|市/g, '').trim();
}
export function prepareRegions(geo) {
  if (geo?.type !== 'FeatureCollection' || !Array.isArray(geo.features)) throw new TypeError('地图不是有效的 GeoJSON FeatureCollection');
  const regions = geo.features.filter(f => f.properties?.name && polygons(f).length).map(f => ({
    id: String(f.properties.adcode ?? f.properties.name), name: f.properties.name,
    short: provinceName(f.properties.name), feature: f, bounds: boundsOf([f]),
    center: f.properties.centroid ?? f.properties.center
  }));
  if (!regions.length) throw new TypeError('地图没有可用的行政区边界');
  return regions;
}
/** Source province strings help resolve border cases, but geometry validates drawable points. */
export function locateProvince(station, regions) {
  const hint = regions.find(r => r.short === provinceName(station.province) || r.id === String(station.province));
  if (!station.coord) return hint?.id ?? null;
  if (hint && inBounds(station.coord, hint.bounds) && pointInFeature(station.coord, hint.feature)) return hint.id;
  const found = regions.find(r => inBounds(station.coord, r.bounds) && pointInFeature(station.coord, r.feature));
  return found?.id ?? hint?.id ?? null;
}
/** Main view + explicit inset: retain all outlying polygon components, never delete them. */
export function splitOffshore(features) {
  const main = [], offshore = [];
  for (const feature of features) {
    const a = [], b = [];
    for (const polygon of polygons(feature)) {
      (polygon[0].every(p => p[1] < 17.8) ? b : a).push(polygon);
    }
    const withPolygons = coordinates => ({...feature, geometry:{type:'MultiPolygon', coordinates}});
    if (a.length) main.push(withPolygons(a));
    if (b.length) offshore.push(withPolygons(b));
  }
  return {main, offshore};
}
/** Equirectangular + affine oblique projection. z is a screen-space extrusion, NOT altitude. */
export function createProjection(features, rect, view = '2d', camera = {zoom:1, pan:[0,0]}) {
  const b = boundsOf(features);
  if (!b || rect.width <= 0 || rect.height <= 0) return null;
  const origin = [(b[0]+b[2])/2, (b[1]+b[3])/2];
  const longitudeScale = Math.cos(origin[1] * Math.PI / 180);
  const angle = view === '2.5d' ? -9 * Math.PI / 180 : 0;
  const tilt = view === '2.5d' ? .73 : 1;
  const co = Math.cos(angle), si = Math.sin(angle);
  const raw = ([lng,lat]) => {
    const x = (lng-origin[0])*longitudeScale, y = -(lat-origin[1]);
    return [co*x-si*y, (si*x+co*y)*tilt];
  };
  const rb = [Infinity,Infinity,-Infinity,-Infinity];
  for (const f of features) for(const p of polygons(f)) for(const r of p) for(const coord of r) {
    const [x,y]=raw(coord); rb[0]=Math.min(rb[0],x);rb[1]=Math.min(rb[1],y);rb[2]=Math.max(rb[2],x);rb[3]=Math.max(rb[3],y);
  }
  const extent = [(rb[0]+rb[2])/2,(rb[1]+rb[3])/2];
  const scale = Math.min(rect.width / Math.max(.001,rb[2]-rb[0]), rect.height / Math.max(.001,rb[3]-rb[1])) * clamp(camera.zoom,1,24);
  const center = [rect.x+rect.width/2+camera.pan[0], rect.y+rect.height/2+camera.pan[1]];
  return {
    scale, center, bounds:b,
    project(coord, z=0) {const p=raw(coord);return [center[0]+(p[0]-extent[0])*scale,center[1]+(p[1]-extent[1])*scale-z];},
    invert([px,py], z=0) {
      const u=(px-center[0])/scale+extent[0], v=((py+z-center[1])/scale+extent[1])/tilt;
      return [origin[0]+(co*u+si*v)/longitudeScale, origin[1]-(-si*u+co*v)];
    }
  };
}
export function haversine(a,b) {
  const rad = Math.PI/180;
  const lat = (b[1]-a[1])*rad, lng=(b[0]-a[0])*rad;
  const h=Math.sin(lat/2)**2+Math.cos(a[1]*rad)*Math.cos(b[1]*rad)*Math.sin(lng/2)**2;
  return 6371*2*Math.asin(Math.min(1,Math.sqrt(h)));
}
/** Geographic-distance-limited, bounded-degree undirected proximity graph. Not power-grid links.
 *  Spatial buckets plus a candidate cap bound dense-cell work. This is approximate nearest-neighbor,
 *  not a claim of globally exact k-NN or worst-case unbounded scans. */
export function proximityEdges(stations, {maxKm=150, maxDegree=3, candidateCap=72}={}) {
  if(maxKm<=0||maxDegree<=0) return [];
  const valid=stations.filter(s=>s.coord?.every(Number.isFinite)).slice().sort((a,b)=>String(a.id).localeCompare(String(b.id)));
  const cell=maxKm/111, buckets=new Map();
  for(const s of valid){const key=`${Math.floor(s.coord[0]/cell)}:${Math.floor(s.coord[1]/cell)}`;if(!buckets.has(key))buckets.set(key,[]);buckets.get(key).push(s);}
  const proposed=new Map();
  for(const a of valid){
    const x=Math.floor(a.coord[0]/cell),y=Math.floor(a.coord[1]/cell),span=Math.min(8,Math.ceil(1/Math.max(.15,Math.cos(a.coord[1]*Math.PI/180))))+1;
    const near=[];
    for(let dx=-span;dx<=span;dx++)for(let dy=-1;dy<=1;dy++){
      const bin=buckets.get(`${x+dx}:${y+dy}`);if(!bin)continue;
      const limit=Math.min(bin.length,candidateCap), offset=hash(a.id)%bin.length;
      for(let k=0;k<limit;k++){
        const b=bin[(offset+Math.floor(k*bin.length/limit))%bin.length]; if(a.id===b.id)continue;
        const km=haversine(a.coord,b.coord); if(km<=maxKm&&km>0.001)near.push({a,b,km});
      }
    }
    near.sort((x,y)=>x.km-y.km);
    for(const e of near.slice(0,maxDegree*2)){const key=JSON.stringify([String(e.a.id),String(e.b.id)].sort());if(!proposed.has(key))proposed.set(key,e);}
  }
  const degree=new Map(), result=[];
  for(const edge of [...proposed.values()].sort((a,b)=>a.km-b.km)){
    if((degree.get(edge.a.id)??0)>=maxDegree||(degree.get(edge.b.id)??0)>=maxDegree)continue;
    degree.set(edge.a.id,(degree.get(edge.a.id)??0)+1);degree.set(edge.b.id,(degree.get(edge.b.id)??0)+1);result.push(edge);
  }
  return result;
}
export function stationPulse(id, seconds, reduced=false) {
  const seed=hash(id), phase=(seed%6283)/1000, period=2.9+(seed%230)/100;
  const breathe=reduced?.65:.5+.5*Math.sin(seconds*Math.PI*2/period+phase);
  const shimmer=reduced?0:Math.pow(Math.max(0,Math.sin(seconds*1.23+phase*1.7)),14);
  return {alpha:.5+.38*breathe, radius:1.9+.65*breathe, glow:10+5*breathe+2*shimmer, shimmer};
}
