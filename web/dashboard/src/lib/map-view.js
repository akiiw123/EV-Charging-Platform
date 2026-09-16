/**
 * 功能：在三层 Canvas 上绘制可缩放、可选省、可点击站点的全国地图。
 * 输入：MapPanel 提供的静态站点、主题和交互回调，以及本地中国 GeoJSON。
 * 输出/接口：createMap 返回 update/theme/setView/setLayout/zoom/fit/dispose 等控制方法。
 */
import {MAP_CONFIG as CONFIG} from './map-config.js';
import {polygons, boundsOf, prepareRegions, pointInFeature, locateProvince, splitOffshore,
  createProjection, proximityEdges, stationPulse, hash, clamp} from './geo-utils.js';

/** Two projection modes share one geometry / picking pipeline.
 * Three cached canvas layers: terrain (static), nearby links (static), station lights (animated).
 * Every light is a supplied station. No random fill, generated city texture, or API fallback.
 */
export function createMap(_echarts, onSelect, {onScopeChange=()=>{}}={}) {
  const $=id=>document.getElementById(id);
  const host=$('map'), stage=host.parentElement, backdrop=$('mapBackdrop'), message=$('mapMessage');
  const controller=new AbortController(), signal=controller.signal;
  const motion=matchMedia('(prefers-reduced-motion: reduce)');
  const desktop=matchMedia('(min-width:1101px) and (min-height:681px)');
  const canvases=['terrain','links','lights'].map(name=>{
    const canvas=document.createElement('canvas');canvas.className=`map-canvas map-${name}`;canvas.setAttribute('aria-hidden','true');host.append(canvas);return canvas;
  });
  const [terrain,links,lights]=canvases.map(c=>c.getContext('2d'));
  if(!terrain||!links||!lights) throw new Error('浏览器不支持 Canvas 2D');
  const tooltip=document.createElement('div');tooltip.className='geo-tooltip';tooltip.hidden=true;tooltip.setAttribute('role','status');host.append(tooltip);
  let geo=null,regions=[],stations=[],provinceFor=new Map(),selected=null,filter='all',view='2d',layout='immersive';
  let camera={zoom:1,pan:[0,0]},nationCamera=null,projection=null,insetProjection=null,insetRect=null;
  let shapes=[],offshoreShapes=[],points=[],edges=[],displayEdges=[],dirty=true,raf=0,lastFrame=0,disposed=false,loaded=false;
  let width=0,height=0,dpr=1,hoverRegion=null,hoverStation=null,showLinks=false,animate=false,mainRect=null;
  let networkDirty=true,stats={staticRenders:0,lightFrames:0,gestureFrames:0},lastPublished='',wasVisible=true;
  const pointerState=new Map();let dragOrigin=null,pinchOrigin=null,dragged=false;
  const sprites=new Map();
  let renderCamera={zoom:1,pan:[0,0]},gesturing=false,gestureRAF=0,settleTimer=0,hoverRAF=0,hoverPos=null;
  let splitCache=null,splitKey=null,pointBins=new Map();
  function scene(){if(splitKey!==selected||!splitCache){splitCache=splitOffshore(activeFeatures());splitKey=selected;}return splitCache;}
  function beginGesture(){
    if(!projection)return;
    clearTimeout(settleTimer);gesturing=true;tooltip.hidden=true;
    host.classList.add('map-gesturing');
    if(raf){cancelAnimationFrame(raf);raf=0;}
  }
  function paintGesture(){
    gestureRAF=0;if(!gesturing||!projection)return;
    const ratio=camera.zoom/renderCamera.zoom;
    const origin=projection.center.map((v,i)=>v-renderCamera.pan[i]);
    const offset=origin.map((v,i)=>v+camera.pan[i]-ratio*(v+renderCamera.pan[i]));
    for(const canvas of canvases)canvas.style.transform=`translate(${offset[0]}px,${offset[1]}px) scale(${ratio})`;
    stats.gestureFrames++;
  }
  function moveGesture(){beginGesture();if(!gestureRAF)gestureRAF=requestAnimationFrame(paintGesture);}
  function settleGesture(){
    clearTimeout(settleTimer);if(gestureRAF)cancelAnimationFrame(gestureRAF);gestureRAF=0;
    if(!gesturing)return;gesturing=false;
    host.classList.remove('map-gesturing');
    for(const canvas of canvases)canvas.style.transform='';
    dirty=true;schedule();
  }
  function endSoon(){clearTimeout(settleTimer);settleTimer=setTimeout(settleGesture,140);}

  const query=new URLSearchParams(location.search);
  if(['2d','2.5d'].includes(query.get('view')))view=query.get('view');
  if(['panels','immersive'].includes(query.get('layout')))layout=query.get('layout');
  const requestedProvince=query.get('province');

  function effectiveImmersive(){return layout==='immersive'&&desktop.matches;}
  function scopedStations(){return stations.filter(s=>!selected||provinceFor.get(s.id)===selected);}
  function candidates(){return scopedStations().filter(s=>filter!=='attention'||s.attention>0||s.issues?.length>0);}
  function activeFeatures(){return selected?[regions.find(r=>r.id===selected).feature]:geo?.features??[];}
  function color(){return document.documentElement.dataset.theme==='day'?{
    land:'#d3e6f0',land2:'#e9f3f6',border:'#719aaf',rim:'#438ea7',wall:'#86b6ca',wallBottom:'#47788e',label:'#456577',line:'0,157,135',hover:'#b3d8e7',gold:'#008f7d',day:true
  }:{land:'#092542',land2:'#08152a',border:'#254666',rim:'#61b9ee',wall:'#1e6492',wallBottom:'#07263f',label:'#9eb6cc',line:'244,201,119',hover:'#133d5d',gold:'#ffde9e',day:false};}
  function makePath(feature,project){
    const path=new Path2D();
    for(const poly of polygons(feature))for(const ring of poly){
      ring.forEach((coord,i)=>{const p=project(coord);if(i===0)path.moveTo(...p);else path.lineTo(...p);});path.closePath();
    }
    return path;
  }
  function publish(){
    const scoped=scopedStations(),visible=candidates();
    const state={province:selected,name:regions.find(r=>r.id===selected)?.name??'全国',stations:scoped.length,drawn:points.length,links:displayEdges.length,view,layout};
    host.dataset.scope=selected??'national';host.dataset.view=view;host.dataset.renderedStations=String(points.length);host.dataset.regionCount=String(selected?1:regions.length);host.dataset.links=String(displayEdges.length);
    $('mapScope').textContent=state.name;
    $('provinceSelect').value=selected??'';
    $('backNational').hidden=!selected;
    $('mapViewName').textContent=view==='2.5d'?'2.5D 立体地图':'2D 俯瞰地图';
    if ($('lightCount')) $('lightCount').textContent=`${points.length.toLocaleString('zh-CN')} 个站点灯光`;
    if ($('linkCount')) $('linkCount').hidden=!showLinks;if (document.querySelector('.legend-disclaimer')) document.querySelector('.legend-disclaimer').hidden=!showLinks;
    if ($('linkCount')) $('linkCount').textContent=showLinks?`${displayEdges.length.toLocaleString('zh-CN')} 条近邻线 · ≤ ${CONFIG.network.maxKm} km`:'近邻连线已关闭';
    const unlocated=visible.filter(s=>!s.coord||!provinceFor.get(s.id)).length;
    if ($('scopeHint')) $('scopeHint').textContent=selected?'地图显示本省站点；分析指标仍为全平台':'点击省域进入 · 点站点查看详情 · 拖动 / 缩放';
    if ($('geometryNote')) $('geometryNote').textContent=unlocated?`${unlocated} 个站点省域未匹配，按原始坐标显示`:'省级底图 · 无地市 / 道路分界';
    const fingerprint=JSON.stringify([selected,stations.length,filter]);
    if(fingerprint!==lastPublished){lastPublished=fingerprint;onScopeChange(state);}
    if(loaded&&(!visible.length||!points.length)){
      message.hidden=false;
      message.textContent=!visible.length?(stations.length?'当前省域 / 筛选下暂无站点，仍可浏览地图':'数据库暂无站点，仍可浏览地图'):'当前范围没有可绘制的站点坐标';
    } else if(geo) message.hidden=true;
  }
  function getRect(){
    const pad=width<600?18:32;
    if(!effectiveImmersive())return {x:pad,y:22,width:Math.max(40,width-pad*2),height:Math.max(50,height-92)};
    const r=stage.getBoundingClientRect(),h=host.getBoundingClientRect();
    // Geometry fits the unobstructed center, while the canvas itself spans the entire viewport.
    if(document.documentElement.dataset.cards==='collapsed')return {x:width*.09,y:height*.19,width:width*.82,height:height*.68};
    if(!selected){const f=CONFIG.cinematicFrame;return {x:width*f.x,y:height*f.y,width:width*f.width,height:height*f.height};}
    return {x:r.left-h.left+16,y:r.top-h.top+5,width:r.width-32,height:Math.max(90,r.height-43)};
  }
  function syncSize(){
    if(disposed)return;
    const r=host.getBoundingClientRect();if(!r.width||!r.height)return;
    const nextDpr=Math.min(devicePixelRatio||1,CONFIG.renderer.maxDPR,Math.sqrt(CONFIG.renderer.pixelBudget/(r.width*r.height)));
    const changed=width!==r.width||height!==r.height||dpr!==nextDpr;
    width=r.width;height=r.height;dpr=nextDpr;
    if(changed)canvases.forEach(canvas=>{canvas.width=Math.round(width*dpr);canvas.height=Math.round(height*dpr);});
    dirty=true;schedule();
  }
  function sprite(day){
    const key=day?'day':'night';if(sprites.has(key))return sprites.get(key);
    const c=document.createElement('canvas');c.width=c.height=80;const x=c.getContext('2d');
    const g=x.createRadialGradient(40,40,0,40,40,40);
    if(day){g.addColorStop(0,'rgba(159,255,220,.98)');g.addColorStop(.13,'rgba(0,189,145,.8)');g.addColorStop(.38,'rgba(0,169,164,.38)');g.addColorStop(1,'rgba(0,169,164,0)');}
    else{g.addColorStop(0,'rgba(255,246,199,1)');g.addColorStop(.10,'rgba(255,218,136,.95)');g.addColorStop(.26,'rgba(255,183,66,.40)');g.addColorStop(.53,'rgba(239,150,35,.13)');g.addColorStop(1,'rgba(239,150,35,0)');}
    x.fillStyle=g;x.fillRect(0,0,80,80);sprites.set(key,c);return c;
  }
  function clear(ctx){ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,width,height);ctx.globalAlpha=1;ctx.globalCompositeOperation='source-over';ctx.shadowBlur=0;}
  function drawGround(c){
    if(view!=='2.5d')return;
    const center=[mainRect.x+mainRect.width/2,mainRect.y+mainRect.height*.57];
    terrain.save();terrain.translate(...center);
    const radius=Math.min(mainRect.width*.5,mainRect.height*.9);
    for(let i=0;i<3;i++){
      terrain.beginPath();terrain.ellipse(0,25,radius+i*18,(radius+i*18)*.43,0,0,Math.PI*2);
      terrain.strokeStyle=c.day?'rgba(52,103,137,.13)':`rgba(68,145,204,${.15-i*.036})`;
      terrain.lineWidth=i===0?1.4:.7;terrain.setLineDash(i===1?[2,10]:[]);terrain.stroke();
    }
    terrain.setLineDash([]);terrain.restore();
  }
  function drawShape(s,c,isInset=false){
    const ctx=terrain, path=s.path;
    const depth=view==='2.5d'&&!isInset?clamp(mainRect.width*CONFIG.extrusion.relativeDepth,CONFIG.extrusion.minDepth,CONFIG.extrusion.maxDepth):0;
    const hovered=hoverRegion===s.id&&!selected;
    const lift=hovered&&depth?3:0;
    ctx.save();ctx.translate(0,-lift);
    if(depth){
      // Cached static extrusion passes. These are not repeated by the light-animation loop.
      ctx.save();ctx.translate(4,depth+9);ctx.shadowColor=c.day?'#21476166':'#010713';ctx.shadowBlur=19;ctx.fillStyle=c.day?'#52738644':'#010611dd';ctx.fill(path,'evenodd');ctx.restore();
      for(let z=depth;z>=1;z-=2){
        ctx.save();ctx.translate(0,z);
        ctx.fillStyle=z>depth*.6?c.wallBottom:c.wall;ctx.fill(path,'evenodd');
        if(z===depth){ctx.strokeStyle=c.rim;ctx.lineWidth=.7;ctx.globalAlpha=.6;ctx.stroke(path);}
        ctx.restore();
      }
      ctx.save();ctx.shadowBlur=10;ctx.shadowColor=c.rim;ctx.strokeStyle=c.day?'#508da1':'#53b8fa99';ctx.lineWidth=1.6;ctx.stroke(path);ctx.restore();
    }
    const gradient=ctx.createLinearGradient(0,mainRect.y,0,mainRect.y+mainRect.height);
    gradient.addColorStop(0,hovered?c.hover:c.land);gradient.addColorStop(1,c.land2);
    ctx.fillStyle=gradient;ctx.fill(path,'evenodd');
    // A dark seam and a thin inner rim read as separate province slabs without moving source coordinates.
    if(depth){ctx.strokeStyle=c.day?'#295b7144':'#020d20';ctx.lineWidth=2.8;ctx.stroke(path);}
    ctx.strokeStyle=hovered?c.rim:c.border;ctx.lineWidth=depth?1:.7;ctx.stroke(path);
    if(depth){ctx.save();ctx.globalAlpha=.45;ctx.translate(0,-.6);ctx.strokeStyle=c.rim;ctx.lineWidth=.55;ctx.stroke(path);ctx.restore();}
    ctx.restore();
  }
  function drawLabels(c){
    const occupied=[];
    for(const s of shapes.slice().sort((a,b)=>b.area-a.area)){
      const region=regions.find(r=>r.id===s.id);if(!region?.center)continue;
      const pos=projection.project(region.center);
      if(pos[0]<4||pos[0]>width-4||pos[1]<10||pos[1]>height-20)continue;
      if(selected)continue; // The province is already named in the prominent map header.
      const size=effectiveImmersive()?11:9;
      const tw=region.short.length*size+12, box=[pos[0]-tw/2,pos[1]-7,tw,15];
      if(occupied.some(b=>box[0]<b[0]+b[2]&&box[0]+box[2]>b[0]&&box[1]<b[1]+b[3]&&box[1]+box[3]>b[1]))continue;
      occupied.push(box);terrain.font=`${size}px "Noto Sans CJK SC","Microsoft YaHei",sans-serif`;
      terrain.textAlign='center';terrain.textBaseline='middle';terrain.lineWidth=3;terrain.strokeStyle=c.day?'#e5eff4bb':'#07172ad9';
      terrain.strokeText(region.short,...pos);terrain.fillStyle=c.label;terrain.fillText(region.short,...pos);
    }
  }
  function drawInset(c,features){
    insetRect=null;insetProjection=null;offshoreShapes=[];
    if(!features.length)return;
    const iw=effectiveImmersive()?92:64,ih=effectiveImmersive()?113:82;
    insetRect={x:mainRect.x+mainRect.width-iw,y:mainRect.y+mainRect.height-ih+15,width:iw,height:ih};
    if(effectiveImmersive()){
      const r=stage.getBoundingClientRect(),h=host.getBoundingClientRect();
      insetRect.x=r.right-h.left-iw-24;insetRect.y=r.bottom-h.top-ih-46;
    }
    terrain.fillStyle=c.day?'#eff6fae8':'#050f20bb';terrain.fillRect(insetRect.x,insetRect.y,iw,ih);
    terrain.strokeStyle=c.border;terrain.lineWidth=.65;terrain.strokeRect(insetRect.x,insetRect.y,iw,ih);
    const r={x:insetRect.x+7,y:insetRect.y+6,width:iw-14,height:ih-29};
    insetProjection=createProjection(features,r,'2d');
    for(const f of features){const s={id:String(f.properties.adcode),path:makePath(f,insetProjection.project)};offshoreShapes.push(s);drawShape(s,c,true);}
    terrain.fillStyle=c.label;terrain.font='9px "Microsoft YaHei",sans-serif';terrain.textAlign='center';terrain.fillText('南海诸岛 · 附图',insetRect.x+iw/2,insetRect.y+ih-8);
  }
  function projectStation(station){
    if(!station.coord)return null;
    const offshore=station.coord[1]<17.8&&insetProjection;
    const p=(offshore?insetProjection:projection).project(station.coord);
    if(!offshore&&hoverRegion===provinceFor.get(station.id)&&view==='2.5d'&&!selected)p[1]-=3;
    return p;
  }
  function rebuild(){
    const started=performance.now();
    if(!geo||!width||!height)return;
    clear(terrain);clear(links);
    const c=color(),split=scene();
    mainRect=getRect();projection=createProjection(split.main,mainRect,view,camera);if(!projection)return;
    shapes=split.main.map(feature=>{
      const b=boundsOf([feature]);return {feature,id:String(feature.properties.adcode),path:makePath(feature,projection.project),area:(b[2]-b[0])*(b[3]-b[1]),bottom:projection.project([b[2],b[1]])[1]};
    }).sort((a,b)=>a.bottom-b.bottom);
    drawGround(c);for(const shape of shapes)drawShape(shape,c);drawLabels(c);drawInset(c,split.offshore);
    const candidate=candidates();
    const drawable=candidate.filter(s=>s.coord&&(!selected||pointInFeature(s.coord,regions.find(r=>r.id===selected).feature)));
    if(networkDirty&&showLinks){edges=proximityEdges(drawable,CONFIG.network);networkDirty=false;}
    points=drawable.map(station=>({station,pos:projectStation(station)})).filter(p=>p.pos&&p.pos[0]>=0&&p.pos[0]<=width&&p.pos[1]>=0&&p.pos[1]<=height);
    pointBins=new Map();
    for(const point of points){const key=`${Math.floor(point.pos[0]/24)}:${Math.floor(point.pos[1]/24)}`;if(!pointBins.has(key))pointBins.set(key,[]);pointBins.get(key).push(point);}
    renderCamera={zoom:camera.zoom,pan:[...camera.pan]};
    const positions=new Map(points.map(p=>[p.station.id,p.pos]));
    displayEdges=edges.filter(e=>positions.has(e.a.id)&&positions.has(e.b.id)&&((e.a.coord[1]<17.8)===(e.b.coord[1]<17.8))).map(e=>({...e,aPos:positions.get(e.a.id),bPos:positions.get(e.b.id)}));
    const density=new Map();
    for(const p of points){const key=`${Math.floor(p.pos[0]/CONFIG.lighting.densityCell)}:${Math.floor(p.pos[1]/CONFIG.lighting.densityCell)}`;density.set(key,(density.get(key)??0)+1);p.densityKey=key;}
    points.forEach(p=>p.gain=1/Math.pow(density.get(p.densityKey),CONFIG.lighting.densityExponent));
    if(showLinks){
      links.lineWidth=.7;links.strokeStyle=`rgba(${c.line},${c.day?.25:.24})`;links.beginPath();
      for(const edge of displayEdges){
        const offshoreA=edge.a.coord[1]<17.8,offshoreB=edge.b.coord[1]<17.8;
        // Never draw a line from the main projection into a geographically displaced inset.
        if(offshoreA!==offshoreB)continue;
        links.moveTo(...edge.aPos);links.lineTo(...edge.bPos);
      }links.stroke();
    }
    stats.staticRenders++;stats.lastStaticMS=Math.round((performance.now()-started)*10)/10;dirty=false;publish();
  }
  function drawLights(seconds){
    const started=performance.now();
    clear(lights);if(!projection)return;
    const c=color(),glow=sprite(c.day),reduced=motion.matches||!animate;
    // Theme-aware station bloom is baked into the small sprite, not shadowBlur per station/frame.
    lights.globalCompositeOperation=c.day?'source-over':'lighter';
    for(const {station,pos:[x,y],gain} of points){
      const pulse=stationPulse(station.id,seconds,reduced);
      const size=pulse.glow*(points.length>CONFIG.lighting.denseThreshold?.62:.85)*(c.day?1.35:1);
      lights.globalAlpha=pulse.alpha*(c.day?.92:CONFIG.lighting.exposure)*gain;lights.drawImage(glow,x-size,y-size,size*2,size*2);
    }
    lights.globalCompositeOperation='source-over';
    for(const {station,pos:[x,y],gain} of points){
      const pulse=stationPulse(station.id,seconds,reduced);
      lights.globalAlpha=pulse.alpha*(.35+.5*gain);
      lights.fillStyle=c.day?'#008f79':'#ffe7a9';lights.beginPath();lights.arc(x,y,points.length>CONFIG.lighting.denseThreshold?.5+pulse.radius*.18:pulse.radius*.58,0,Math.PI*2);lights.fill();
      if(station.attention&&points.length<260){lights.strokeStyle='#fb8c84';lights.lineWidth=.8;lights.beginPath();lights.arc(x,y,4.7,0,Math.PI*2);lights.stroke();}
      // A small minority gets a slow star glint, not a synchronized strobe.
      if(!reduced&&hash(station.id)%11===0&&pulse.shimmer>.35){
        const length=3+pulse.shimmer*5;lights.globalAlpha=pulse.shimmer*.68;lights.strokeStyle=c.gold;lights.lineWidth=.6;
        lights.beginPath();lights.moveTo(x-length,y);lights.lineTo(x+length,y);lights.moveTo(x,y-length*.7);lights.lineTo(x,y+length*.7);lights.stroke();
      }
    }
    if(hoverStation){const p=points.find(p=>p.station.id===hoverStation);if(p){lights.globalAlpha=1;lights.strokeStyle=c.gold;lights.lineWidth=1;lights.beginPath();lights.arc(...p.pos,8,0,Math.PI*2);lights.stroke();}}
    lights.globalAlpha=1;stats.lightFrames++;stats.lastLightsMS=Math.round((performance.now()-started)*10)/10;
  }
  function visible(){const r=host.getBoundingClientRect();return r.bottom>0&&r.top<innerHeight&&r.right>0&&r.left<innerWidth;}
  function tick(now){
    raf=0;if(disposed||document.hidden||gesturing)return;
    const inView=visible();if(!inView){wasVisible=false;return;}
    if(!wasVisible){wasVisible=true;dirty=true;}
    const changed=dirty;if(dirty)rebuild();
    if(changed||now-lastFrame>=1000/CONFIG.renderer.maxFPS||motion.matches||!animate){drawLights(now/1000);lastFrame=now;}
    if(animate&&!motion.matches&&points.length)raf=requestAnimationFrame(tick);
  }
  function schedule(){if(!raf&&!disposed&&!document.hidden)raf=requestAnimationFrame(tick);}
  function invalidate(network=false){settleGesture();dirty=true;networkDirty=networkDirty||network;schedule();}
  function chooseProvince(id){
    const next=regions.find(r=>r.id===String(id)||r.name===id||r.short===id)?.id??null;
    if(next===selected)return;
    if(!selected&&next)nationCamera={zoom:camera.zoom,pan:[...camera.pan]};
    selected=next;camera=!selected&&nationCamera?{zoom:nationCamera.zoom,pan:[...nationCamera.pan]}:{zoom:1,pan:[0,0]};
    hoverRegion=null;hoverStation=null;tooltip.hidden=true;invalidate(true);publish();
  }
  function applyLayout(){
    const target=effectiveImmersive()?backdrop:stage;
    if(host.parentElement!==target)target.prepend(host);
    document.documentElement.dataset.layout=layout;
    if ($('layoutHint')) $('layoutHint').textContent=layout==='immersive'&&!desktop.matches?'窄屏自动使用面板布局':'';
    document.querySelectorAll('button[data-layout]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.layout===layout)));
    // Changing portal size is observed; stage position changes are also measured explicitly.
    syncSize();requestAnimationFrame(syncSize);
  }
  function zoom(factor,anchor=null){
    if(!projection)return;
    const focus=anchor??[mainRect.x+mainRect.width/2,mainRect.y+mainRect.height/2];
    const next=clamp(camera.zoom*factor,1,24),ratio=next/camera.zoom;
    if(ratio===1)return;
    const origin=projection.center.map((v,i)=>v-renderCamera.pan[i]);
    camera.pan=focus.map((v,i)=>v-origin[i]-ratio*(v-origin[i]-camera.pan[i]));
    camera.zoom=next;moveGesture();endSoon();
  }
  function local(e){const r=host.getBoundingClientRect();return [e.clientX-r.left,e.clientY-r.top];}
  function pick(pos){
    let nearest=null,distance=10;
    const bx=Math.floor(pos[0]/24),by=Math.floor(pos[1]/24),near=[];
    for(let x=bx-1;x<=bx+1;x++)for(let y=by-1;y<=by+1;y++)near.push(...(pointBins.get(`${x}:${y}`)??[]));
    for(const p of near){const d=Math.hypot(p.pos[0]-pos[0],p.pos[1]-pos[1]);if(d<distance){distance=d;nearest=p.station;}}
    if(nearest)return {station:nearest};
    if(insetRect&&pos[0]>=insetRect.x&&pos[0]<=insetRect.x+insetRect.width&&pos[1]>=insetRect.y&&pos[1]<=insetRect.y+insetRect.height){
      const coord=insetProjection.invert(pos);const region=regions.find(r=>pointInFeature(coord,r.feature));return region?{region}:{};
    }
    if(projection){const coord=projection.invert(pos);const region=regions.find(r=>(!selected||r.id===selected)&&pointInFeature(coord,r.feature));if(region)return {region};}
    return {};
  }
  function showHover(pos){
    const hit=pick(pos),nextRegion=hit.region?.id??null,nextStation=hit.station?.id??null;
    const stationChanged=hoverStation!==nextStation;
    hoverRegion=nextRegion;hoverStation=nextStation;
    host.style.cursor=hit.station||hit.region&&!selected?'pointer':'grab';
    if(!hit.station&&!hit.region){tooltip.hidden=true;return;}
    const headline=document.createElement('strong'),detail=document.createElement('span');
    headline.textContent=hit.station?.name??hit.region.name;
    detail.textContent=hit.station?'设备数量 / 运行状态未知 · 点击详情':
      `${stations.filter(s=>provinceFor.get(s.id)===hit.region.id).length} 个站点 · ${selected?'省级边界视图':'点击仅查看本省'}`;
    tooltip.replaceChildren(headline,detail);tooltip.hidden=false;
    tooltip.style.left=`${clamp(pos[0]+16,8,width-280)}px`;tooltip.style.top=`${clamp(pos[1]-58,8,height-68)}px`;
    if(stationChanged)schedule();
  }
  host.addEventListener('pointerdown',e=>{
    if(e.button!==0)return;const pos=local(e);host.setPointerCapture(e.pointerId);pointerState.set(e.pointerId,pos);
    dragged=false;dragOrigin={pos,pan:[...camera.pan]};
    if(pointerState.size===2){const [a,b]=[...pointerState.values()];pinchOrigin={distance:Math.hypot(a[0]-b[0],a[1]-b[1]),zoom:camera.zoom,pan:[...camera.pan],center:[(a[0]+b[0])/2,(a[1]+b[1])/2]};dragged=true;}
    tooltip.hidden=true;
  },{signal});
  host.addEventListener('pointermove',e=>{
    const pos=local(e);
    if(!pointerState.has(e.pointerId)){if(e.pointerType!=='touch'&&!gesturing){hoverPos=pos;if(!hoverRAF)hoverRAF=requestAnimationFrame(()=>{hoverRAF=0;if(!gesturing)showHover(hoverPos);});}return;}
    pointerState.set(e.pointerId,pos);
    if(pointerState.size===2&&pinchOrigin){const [a,b]=[...pointerState.values()],distance=Math.hypot(a[0]-b[0],a[1]-b[1]);camera.zoom=clamp(pinchOrigin.zoom*distance/Math.max(1,pinchOrigin.distance),1,24);const ratio=camera.zoom/pinchOrigin.zoom,origin=projection.center.map((v,i)=>v-renderCamera.pan[i]),center=[(a[0]+b[0])/2,(a[1]+b[1])/2];camera.pan=center.map((v,i)=>v-origin[i]-ratio*(pinchOrigin.center[i]-origin[i]-pinchOrigin.pan[i]));dragged=true;moveGesture();return;}
    if(dragOrigin){const dx=pos[0]-dragOrigin.pos[0],dy=pos[1]-dragOrigin.pos[1];if(Math.hypot(dx,dy)>4)dragged=true;
      if(dragged){camera.pan=[dragOrigin.pan[0]+dx,dragOrigin.pan[1]+dy];host.style.cursor='grabbing';moveGesture();}}
  },{signal});
  function release(e){
    const wasDragging=dragged,known=pointerState.has(e.pointerId);pointerState.delete(e.pointerId);
    if(host.hasPointerCapture(e.pointerId))host.releasePointerCapture(e.pointerId);
    if(!pointerState.size){dragOrigin=null;pinchOrigin=null;settleGesture();host.style.cursor='grab';}
    else{const pos=[...pointerState.values()][0];dragOrigin={pos,pan:[...camera.pan]};pinchOrigin=null;}
    if(!wasDragging&&known&&e.type==='pointerup'){const hit=pick(local(e));if(hit.station)onSelect(hit.station);else if(hit.region&&!selected)chooseProvince(hit.region.id);}
  }
  host.addEventListener('pointerup',release,{signal});host.addEventListener('pointercancel',release,{signal});host.addEventListener('lostpointercapture',release,{signal});
  host.addEventListener('pointerleave',()=>{if(!pointerState.size){tooltip.hidden=true;hoverRegion=null;hoverStation=null;schedule();}},{signal});
  host.addEventListener('wheel',e=>{e.preventDefault();const pixels=e.deltaY*(e.deltaMode===1?16:e.deltaMode===2?height:1);zoom(Math.exp(-clamp(pixels,-100,100)*.002),local(e));},{passive:false,signal});
  host.addEventListener('keydown',e=>{
    if(e.key==='Escape'){chooseProvince(null);return;}
    if(['+','=','-'].includes(e.key)){e.preventDefault();zoom(e.key==='-'?1/1.3:1.3);}
    const delta={ArrowLeft:[-30,0],ArrowRight:[30,0],ArrowUp:[0,-30],ArrowDown:[0,30]}[e.key];
    if(delta){e.preventDefault();camera.pan=camera.pan.map((v,i)=>v+delta[i]);moveGesture();endSoon();}
  },{signal});
  function motionChange(){
    if ($('toggleMotion')) $('toggleMotion').textContent=motion.matches?'静态 · 系统偏好':animate?'呼吸灯光':'静态灯光';
    if ($('toggleMotion')) $('toggleMotion').title=motion.matches?'系统要求减少动态效果，灯光保持静态':'';
    if(raf)cancelAnimationFrame(raf);raf=0;drawLights(0);schedule();}
  motion.addEventListener('change',motionChange);
  const desktopChange=()=>applyLayout();desktop.addEventListener('change',desktopChange);
  document.addEventListener('visibilitychange',()=>{if(document.hidden){cancelAnimationFrame(raf);raf=0;}else{lastFrame=0;schedule();}},{signal});
  window.addEventListener('scroll',schedule,{passive:true,signal});
  window.addEventListener('resize',syncSize,{passive:true,signal});
  const ro=new ResizeObserver(()=>syncSize());ro.observe(host);ro.observe(stage);
  const io=new IntersectionObserver(entries=>{if(entries[0]?.isIntersecting)schedule();});io.observe(host);
  const timeout=setTimeout(()=>controller.abort(),12000);
  const ready=fetch('./assets/china.json',{signal}).then(r=>{if(!r.ok)throw new Error(`HTTP ${r.status}`);return r.json();}).then(data=>{
    if(disposed)return [];
    geo=data;regions=prepareRegions(data);
    const options=document.createDocumentFragment();const all=document.createElement('option');all.value='';all.textContent='全国 · 选择省份';options.append(all);
    regions.forEach(r=>{const o=document.createElement('option');o.value=r.id;o.textContent=r.name;options.append(o);});$('provinceSelect').replaceChildren(options);
    provinceFor=new Map(stations.map(s=>[s.id,locateProvince(s,regions)]));
    if(requestedProvince)chooseProvince(requestedProvince);
    invalidate(true);return regions;
  }).catch(error=>{
    if(!disposed){message.hidden=false;message.textContent=`本地地图不可用：${error.name==='AbortError'?'加载超时':error.message}；站点列表仍可使用。`;}
    return [];
  }).finally(()=>clearTimeout(timeout));
  applyLayout();motionChange();
  return {
    ready,
    update(data){loaded=true;stations=data.stations;provinceFor=new Map(stations.map(s=>[s.id,locateProvince(s,regions)]));lastPublished='';invalidate(true);publish();},
    theme(){invalidate();},
    setFilter(value){filter=value;invalidate(true);publish();},
    setView(value){if(!['2d','2.5d'].includes(value))return;view=value;hoverRegion=null;invalidate();publish();},
    setLayout(value){if(!['panels','immersive'].includes(value))return;layout=value;applyLayout();},
    setLinks(value){showLinks=Boolean(value);invalidate();},
    setMotion(value){animate=Boolean(value);motionChange();},
    selectProvince:chooseProvince,
    getScopedStations:scopedStations,
    resize:syncSize,
    reset(){if(selected)chooseProvince(null);else{camera={zoom:1,pan:[0,0]};invalidate();}},
    fit(){camera={zoom:1,pan:[0,0]};invalidate();},
    zoom,
    debug(){return {ready:!!geo,selected,view,layout,effectiveImmersive:effectiveImmersive(),camera,regionCount:selected?1:regions.length,
      regions:regions.map(r=>({id:r.id,name:r.name,position:projection&&r.center?projection.project(r.center):null})),
      stations:points.map(p=>({id:p.station.id,pos:p.pos,province:provinceFor.get(p.station.id)})),edges:displayEdges.length,
      activeFeatureIds:shapes.map(s=>s.id),offshoreFeatureIds:offshoreShapes.map(s=>s.id),stats:{...stats},gesturing,motionReduced:motion.matches,animated:animate,rafActive:!!raf,mainRect,canvasSize:[width,height],dpr};},
    dispose(){clearTimeout(settleTimer);cancelAnimationFrame(gestureRAF);cancelAnimationFrame(hoverRAF);disposed=true;controller.abort();clearTimeout(timeout);cancelAnimationFrame(raf);ro.disconnect();io.disconnect();motion.removeEventListener('change',motionChange);desktop.removeEventListener('change',desktopChange);canvases.forEach(c=>{c.width=c.height=0;c.remove();});tooltip.remove();sprites.clear();}
  };
}
