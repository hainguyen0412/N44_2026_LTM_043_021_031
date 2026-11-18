'use strict';
(async () => {
  const root = document.getElementById('ltm-mission-layout');
  const q = s => root.querySelector(s);
  const iconText={'radio-tower':'◉','map-pin-plus':'+','upload':'↑','play':'▶','drone':'✦'};
  root.querySelectorAll('[data-lucide]').forEach(el=>{el.textContent=iconText[el.dataset.lucide]||'';});
  const number = (n, digits = 1) => Number.isFinite(n) ? n.toLocaleString('vi-VN', {maximumFractionDigits: digits}) : '—';
  let csrf = '', owner = sessionStorage.getItem('mavlink_owner') || '';
  let state = {}, map, points = [], selected = null, adding = false, nextId = 1, command = '', revision = 0;
  let backendAvailable = false, suppressClickUntil = 0;
  let pendingAction = false, lastRequestError = '', maxWaypoints = 97;
  const view = {zoom:1,x:0,y:0};
  function applyView() {
    const el=q('#mc-map');
    view.x=Math.max(-el.offsetWidth*(view.zoom-1)/2,Math.min(el.offsetWidth*(view.zoom-1)/2,view.x));
    view.y=Math.max(-el.offsetHeight*(view.zoom-1)/2,Math.min(el.offsetHeight*(view.zoom-1)/2,view.y));
    el.style.transform='translate('+view.x+'px,'+view.y+'px) scale('+view.zoom+')';
    q('#mc-zoom-reset').textContent=Math.round(view.zoom*100)+'%';
  }
  const id = () => crypto.randomUUID();
  const homePoint = () => state.map_home || ({x: map.gate.x, y: map.gate.y});
  const canEdit = () => backendAvailable && (!state.connected || (state.role==='controller' && !!owner)) && !pendingAction && !['uploading','reading'].includes(state.mission?.state) && !state.telemetry?.heartbeat?.armed;
  const canControl = () => backendAvailable && state.connected && state.role === 'controller' && !!owner && state.fresh;
  const local = p => ({east: (p.x - map.gate.x) * map.scale, north: (map.gate.y - p.y) * map.scale});
  function message(text) { q('#mc-map-note').textContent = text; }
  async function api(path, body) {
    const response = await fetch(path, {
      method: body === undefined ? 'GET' : 'POST',
      headers: {'Content-Type':'application/json', 'X-CSRF-Token':csrf, 'X-Controller-Token':owner},
      body: body === undefined ? undefined : JSON.stringify(body)
    });
    const value = await response.json();
    if (!response.ok) {
      if (response.status === 403 && value.error === 'controller_owner_required') {
        owner = ''; sessionStorage.removeItem('mavlink_owner');
      }
      throw new Error(value.message || value.error || 'HTTP ' + response.status);
    }
    return value;
  }
  function dirty() { revision++; q('#mc-draft-badge').textContent = 'Bản nháp'; refresh(); }
  function draft() {
    return {schema_version:1, map_id:map.map_id, map_version:map.version,
      takeoff_altitude: Number(q('#mc-takeoff-alt').value),
      points:points.map(({x,y,alt,hold}) => ({x,y,alt,hold})), revision};
  }
  const signature = () => JSON.stringify(draft());
  let uploadedSignature = '', uploadedRequestId = '';
  const svgNS = 'http://www.w3.org/2000/svg';
  function drawCampus() {
    const svg = q('#mc-basemap');
    const shape = (tag, attrs) => { const el = document.createElementNS(svgNS, tag); for (const [k,v] of Object.entries(attrs)) el.setAttribute(k, v); svg.append(el); };
    shape('rect',{width:map.width,height:map.height,class:'campus-land'});
    map.buildings.forEach(b => shape('rect',{x:b.x-3,y:b.y-3,width:b.w+6,height:b.h+6,class:'campus-yard'}));
    map.roads.forEach(r => shape('polyline',{points:r.points.map(p=>p.join(',')).join(' '),'stroke-width':r.width+2.2,class:'campus-curb'}));
    map.roads.forEach(r => {
      const pts = r.points.map(p=>p.join(',')).join(' ');
      shape('polyline',{points:pts,'stroke-width':r.width,class:'campus-road'});
      if (r.dashed) shape('polyline',{points:pts,class:'campus-road-mark'});
    });
    map.trees.forEach(p => shape('polygon',{points:p.map(a=>a.join(',')).join(' '),class:'campus-tree'}));
    map.buildings.forEach(b => shape('rect',{x:b.x,y:b.y,width:b.w,height:b.h,class:'campus-building'}));
    map.labels.forEach(label => {
      const span = document.createElement('span'); span.className='mc-campus-label'; span.textContent=label.text;
      span.style.left=label.x/map.width*100+'%'; span.style.top=label.y/map.height*100+'%'; q('#mc-campus-labels').append(span);
    });
  }
  function mapPoint(e) {
    const rect = q('#mc-map').getBoundingClientRect();
    return {x:Math.max(0,Math.min(map.width,(e.clientX-rect.left)/rect.width*map.width)),
      y:Math.max(0,Math.min(map.height,(e.clientY-rect.top)/rect.height*map.height))};
  }
  function renderRoute() {
    const path = [homePoint(), ...points, homePoint()];
    q('#mc-route-line').setAttribute('points', path.map(p=>(p.x/map.width*100)+','+(p.y/map.height*100)).join(' '));
    const length = path.slice(1).reduce((sum,p,i)=>sum+Math.hypot(p.x-path[i].x,p.y-path[i].y)*map.scale,0);
    q('#mc-distance').textContent='Đường nối: '+number(length,0)+' m';
    q('#mc-route-count').textContent=points.length+'/'+maxWaypoints+' waypoint · Về trên Home → Hạ cánh';
    const home=homePoint(),marker=q('.mc-home'),label=q('.mc-home-label');
    marker.style.left=home.x/map.width*100+'%';marker.style.top=home.y/map.height*100+'%';
    label.style.left='calc('+home.x/map.width*100+'% - 24px)';label.style.right='auto';
    label.style.top=home.y/map.height*100+'%';label.style.transform='translate(-100%,-50%)';
    label.textContent=state.map_home?'Home PX4':'Cổng · chờ Home';
  }
  function renderPoints() {
    const list=q('#mc-items'), markers=q('#mc-markers'); list.replaceChildren(); markers.replaceChildren();
    const fixed=text=>{const li=document.createElement('li');li.className='mc-fixed-item';li.textContent=text;list.append(li);return li;};
    fixed('↑ Cất cánh tại Home · '+number(Number(q('#mc-takeoff-alt').value))+' m');
    points.forEach((p,index)=>{
      const li=document.createElement('li'), button=document.createElement('button'); button.type='button';button.className='mc-item';
      button.textContent=(index+1)+'. Điểm '+(index+1)+' · '+number(p.alt)+' m · dừng '+number(p.hold)+' s';
      button.setAttribute('aria-pressed',p.id===selected?'true':'false');
      button.onclick=()=>{selected=p.id;renderPoints();};li.append(button);list.append(li);
      const marker=document.createElement('button');marker.type='button';marker.className='mc-marker';marker.textContent=index+1;
      marker.dataset.seq=String(index+1);button.dataset.seq=String(index+1);
      marker.style.left=p.x/map.width*100+'%';marker.style.top=p.y/map.height*100+'%';
      marker.setAttribute('aria-label','Điểm '+(index+1));marker.setAttribute('aria-pressed',p.id===selected?'true':'false');
      marker.onclick=e=>{e.stopPropagation();selected=p.id;renderPoints();};
      marker.onpointerdown=e=>{
        e.stopPropagation();if(!canEdit())return;marker.setPointerCapture(e.pointerId);
        const start={x:e.clientX,y:e.clientY};let moved=false;
        const move=ev=>{if(Math.hypot(ev.clientX-start.x,ev.clientY-start.y)<4&&!moved)return;moved=true;Object.assign(p,mapPoint(ev));marker.style.left=p.x/map.width*100+'%';marker.style.top=p.y/map.height*100+'%';renderRoute();};
        const end=()=>{marker.removeEventListener('pointermove',move);marker.removeEventListener('pointerup',end);marker.removeEventListener('pointercancel',end);suppressClickUntil=Date.now()+200;selected=p.id;if(moved)dirty();renderPoints();};
        marker.addEventListener('pointermove',move);marker.addEventListener('pointerup',end);marker.addEventListener('pointercancel',end);
      };
      markers.append(marker);
    });
    fixed('↪ Về trên Home · '+number(points.at(-1)?.alt??0)+' m · dừng 1 s').dataset.seq=String(points.length+1);
    fixed('↓ Hạ thẳng xuống tại Home').dataset.seq=String(points.length+2);
    const p=points.find(p=>p.id===selected);q('#mc-editor').hidden=!p;
    if(p){q('#mc-edit-title').textContent='Sửa điểm '+(points.indexOf(p)+1);q('#mc-alt').value=p.alt;q('#mc-hold').value=p.hold;
      const en=local(p);q('#mc-coordinate').textContent='E '+number(en.east)+' m · N '+number(en.north)+' m so với cổng';}
    renderRoute();refresh();
  }
  function refresh() {
    if(!map)return;
    const t=state.telemetry||{}, hb=t.heartbeat||{}, live=backendAvailable&&state.fresh;
    q('#mc-gateway-status').textContent=!backendAvailable?'○ Web backend mất kết nối':state.connected?'● Gateway đã kết nối':'○ Gateway chưa kết nối';
    q('#mc-px4-status').textContent=live?'● PX4 có heartbeat':'○ PX4 chưa sẵn sàng';
    q('#mc-connect').textContent=state.connected?(state.role==='controller'&&!owner?'Nhận quyền điều khiển':'Ngắt kết nối'):'Kết nối';
    q('#mc-role').disabled=state.connected||pendingAction;
    q('#mc-battery').textContent=number(backendAvailable&&t.battery?.fresh?t.battery.percent:null,0);q('#mc-voltage').textContent=number(backendAvailable&&t.battery?.fresh?t.battery.voltage:null,2)+' V';
    q('#mc-height').textContent=number(backendAvailable&&t.position?.fresh?t.position.relative_altitude:null);
    q('#mc-flight-state').textContent=!live||hb.armed===undefined?'—':hb.armed?'ARMED':'DISARMED';
    q('#mc-control-armed').textContent=q('#mc-flight-state').textContent;
    q('#mc-flight-mode').textContent=live?(hb.mode||'Chưa có chế độ bay'):'Chờ heartbeat mới';
    q('#mc-heartbeat-age').textContent=!backendAvailable?'Không nhận snapshot mới':state.heartbeat_age_ms==null?'Chưa có heartbeat':'Heartbeat '+number(state.heartbeat_age_ms/1000)+' giây trước';
    q('#mc-attitude').textContent=backendAvailable&&t.attitude?.fresh?'Roll '+number(t.attitude.roll)+'° · Pitch '+number(t.attitude.pitch)+'° · Yaw '+number(t.attitude.yaw)+'°':'ATTITUDE chưa có hoặc đã cũ';
    renderRoute();
    const px=state.map_position;
    q('#mc-drone').hidden=!live||!t.position?.fresh||!px||px.x<0||px.x>map.width||px.y<0||px.y>map.height;
    if(px){q('#mc-drone').style.left=px.x/map.width*100+'%';q('#mc-drone').style.top=px.y/map.height*100+'%';}
    root.querySelectorAll('[data-cmd]').forEach(b=>{b.disabled=!canControl()||pendingAction||state.command?.busy||state.mission?.busy||(b.dataset.cmd==='TAKEOFF'&&!hb.armed);});
    q('#mc-command-result').textContent=state.command?.event?state.command.command+' · '+state.command.event+(state.command.result==null?'':' · result='+state.command.result):'Chưa gửi lệnh.';
    const ms=state.mission||{};
    q('#mc-mission-result').textContent=(ms.message||'Chưa tải mission lên PX4.')+
      (ms.consumed&&ms.current>=0?' · Mục '+(ms.current+1)+'/'+ms.total+'; đã tới '+(ms.reached>=0?ms.reached+1:0)+'/'+ms.total: '');
    const matching=uploadedSignature===signature();
    root.querySelectorAll('[data-seq]').forEach(el=>{
      const seq=Number(el.dataset.seq),reached=matching&&ms.consumed&&ms.reached>=seq,current=matching&&ms.consumed&&ms.current===seq;
      el.classList.toggle('mc-reached',!!reached);el.classList.toggle('mc-current',!!current);
      if(el.classList.contains('mc-marker'))el.textContent=(reached?'✓':'')+seq;
    });
    if(state.request_error&&state.request_error!==lastRequestError){lastRequestError=state.request_error;message(state.request_error);}
    q('#mc-log-directory').textContent=state.log_directory?'Log: '+state.log_directory:'Ghi log file đang tắt.';
    q('#mc-draft-badge').textContent=ms.verified&&matching?'PX4 đã xác minh':ms.busy?'Đang trao đổi':'Bản nháp';
    q('#mc-start').disabled=!canControl()||!hb.armed||!ms.verified||!matching||ms.consumed||pendingAction||ms.busy||state.command?.busy;
    q('#mc-upload').disabled=!canControl()||!canEdit()||!points.length||ms.busy||state.command?.busy;
    q('#mc-readback').disabled=!canControl()||ms.busy||!!hb.armed||state.command?.busy;
    q('#mc-add').disabled=!canEdit()||points.length>=maxWaypoints;q('#mc-add').setAttribute('aria-pressed',String(adding));q('#mc-map').dataset.adding=String(adding);
    q('#mc-map-mode').textContent=adding?'Click để thêm waypoint':'Chọn điểm để sửa';
    ['#mc-alt','#mc-hold','#mc-takeoff-alt','#mc-open','#mc-delete'].forEach(s=>q(s).disabled=!canEdit());
    const index=points.findIndex(p=>p.id===selected);
    q('#mc-up').disabled=!canEdit()||index<=0;q('#mc-down').disabled=!canEdit()||index<0||index>=points.length-1;
    const events=state.events||[];const log=q('#mc-events');log.replaceChildren();
    events.slice(-4).reverse().forEach(e=>{const li=document.createElement('li');li.textContent=e.time+' · '+e.message;log.append(li);});
    if(state.connected)q('#mc-role').value=state.role;
    q('#mc-map-note').classList.toggle('stale',!!state.connected&&!state.fresh);
  }
  function tab(name){const mission=name==='mission';q('#mc-tab-mission').setAttribute('aria-selected',String(mission));q('#mc-tab-control').setAttribute('aria-selected',String(!mission));q('#mc-mission-pane').hidden=!mission;q('#mc-control-pane').hidden=mission;}
  q('#mc-zoom-in').onclick=()=>{view.zoom=Math.min(4,view.zoom+.5);applyView();};
  q('#mc-zoom-out').onclick=()=>{view.zoom=Math.max(1,view.zoom-.5);applyView();};
  q('#mc-zoom-reset').onclick=()=>{Object.assign(view,{zoom:1,x:0,y:0});applyView();};
  q('#mc-map-viewport').onpointerdown=e=>{
    if(view.zoom===1||adding||e.button!==0||e.target.closest('.mc-marker'))return;
    const viewport=q('#mc-map-viewport'),start={x:e.clientX,y:e.clientY,dx:view.x,dy:view.y};
    viewport.setPointerCapture(e.pointerId);
    const move=ev=>{view.x=start.dx+ev.clientX-start.x;view.y=start.dy+ev.clientY-start.y;applyView();};
    const end=()=>{viewport.removeEventListener('pointermove',move);viewport.removeEventListener('pointerup',end);viewport.removeEventListener('pointercancel',end);suppressClickUntil=Date.now()+200;};
    viewport.addEventListener('pointermove',move);viewport.addEventListener('pointerup',end);viewport.addEventListener('pointercancel',end);
  };
  q('#mc-tab-mission').onclick=()=>tab('mission');q('#mc-tab-control').onclick=()=>tab('control');
  q('#mc-add').onclick=()=>{adding=!adding;refresh();};
  q('#mc-map').onclick=e=>{if(!adding||!canEdit()||points.length>=maxWaypoints||Date.now()<suppressClickUntil)return;const p={id:nextId++,...mapPoint(e),alt:10,hold:0};points.push(p);selected=p.id;dirty();renderPoints();};
  for(const [selector,key,min,max] of [['#mc-alt','alt',2,100],['#mc-hold','hold',0,60]])q(selector).onchange=()=>{const p=points.find(p=>p.id===selected),value=Number(q(selector).value);if(!p||!canEdit())return;if(!Number.isFinite(value)||q(selector).value===''||value<min||value>max){q(selector).value=p[key];return message('Giá trị phải nằm trong '+min+'–'+max+'.');}p[key]=value;dirty();renderPoints();};
  q('#mc-takeoff-alt').onchange=()=>{dirty();renderPoints();};
  function reorder(offset){const i=points.findIndex(p=>p.id===selected);if(!canEdit()||i<0||i+offset<0||i+offset>=points.length)return;[points[i],points[i+offset]]=[points[i+offset],points[i]];dirty();renderPoints();}
  q('#mc-up').onclick=()=>reorder(-1);q('#mc-down').onclick=()=>reorder(1);
  q('#mc-delete').onclick=()=>{if(!canEdit())return;points=points.filter(p=>p.id!==selected);selected=points[0]?.id;dirty();renderPoints();};
  q('#mc-save').onclick=()=>{const url=URL.createObjectURL(new Blob([JSON.stringify(draft(),null,2)],{type:'application/json'}));const a=document.createElement('a');a.href=url;a.download='dut_mission.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
  q('#mc-open').onclick=()=>q('#mc-file').click();
  q('#mc-file').onchange=async()=>{try{const file=q('#mc-file').files[0];if(!file||file.size>65536)throw Error('File quá lớn (tối đa 64 KB).');const data=JSON.parse(await file.text());if(data.schema_version!==1||data.map_id!==map.map_id||data.map_version!==map.version||!Array.isArray(data.points)||data.points.length>maxWaypoints)throw Error('File không đúng map/schema hoặc quá nhiều điểm.');for(const p of data.points)if(!['x','y','alt','hold'].every(k=>typeof p[k]==='number'&&Number.isFinite(p[k]))||p.x<0||p.x>map.width||p.y<0||p.y>map.height||p.alt<2||p.alt>100||p.hold<0||p.hold>60)throw Error('Waypoint không hợp lệ.');if(!Number.isFinite(data.takeoff_altitude)||data.takeoff_altitude<2||data.takeoff_altitude>100)throw Error('Độ cao cất cánh không hợp lệ.');points=data.points.map(p=>({...p,id:nextId++}));selected=points[0]?.id;q('#mc-takeoff-alt').value=data.takeoff_altitude;dirty();renderPoints();message('Đã mở bản nháp; cần tải lên và xác minh trước khi chạy.');}catch(e){message(e.message);}finally{q('#mc-file').value='';}};
  async function action(fn){if(pendingAction)return;pendingAction=true;refresh();try{await fn();}catch(e){message(e.message);}finally{pendingAction=false;refresh();}}
  q('#mc-connect').onclick=()=>action(async()=>{if(state.connected&&!(state.role==='controller'&&!owner)){await api('/api/disconnect',{});owner='';sessionStorage.removeItem('mavlink_owner');}else{const result=await api('/api/connect',{role:q('#mc-role').value});owner=result.controller_token||'';if(owner)sessionStorage.setItem('mavlink_owner',owner);}state=await api('/api/state');});
  root.querySelectorAll('[data-cmd]').forEach(b=>b.onclick=()=>{command=b.dataset.cmd;q('#mc-confirm-title').textContent='Xác nhận '+command;q('#mc-confirm-text').textContent='Gửi lệnh '+command+' tới PX4 qua gateway. Kiểm tra trạng thái drone trước khi xác nhận.';q('#mc-confirm').hidden=false;});
  q('#mc-start').onclick=()=>{command='MISSION';q('#mc-confirm-title').textContent='Bắt đầu mission?';q('#mc-confirm-text').textContent='Thực hiện '+points.length+' waypoint, về trên Home ở '+number(points.at(-1)?.alt)+' m rồi hạ cánh. Mission đã tải và xác minh; lệnh này có thể làm drone cất cánh.';q('#mc-confirm').hidden=false;};
  q('#mc-confirm-no').onclick=()=>{command='';q('#mc-confirm').hidden=true;};
  q('#mc-confirm-yes').onclick=()=>action(async()=>{q('#mc-confirm').hidden=true;const cmd=command;command='';if(cmd==='MISSION'){if(uploadedSignature!==signature())throw Error('Bản nháp đã thay đổi. Tải lại mission.');await api('/api/missions/start',{request_id:id(),upload_request_id:uploadedRequestId});}else await api('/api/commands',{command:cmd,request_id:id()});});
  q('#mc-upload').onclick=()=>action(async()=>{const requestId=id();await api('/api/missions/upload',{...draft(),request_id:requestId});uploadedSignature=signature();uploadedRequestId=requestId;});
  q('#mc-readback').onclick=()=>action(async()=>{await api('/api/missions/readback',{request_id:id()});});
  try {
    const bootstrap=await api('/api/bootstrap');csrf=bootstrap.csrf;maxWaypoints=bootstrap.max_waypoints;
    map=await api('/api/map');state=await api('/api/state');backendAvailable=true;
    q('#mc-basemap').setAttribute('viewBox','0 0 '+map.width+' '+map.height);
    points=[{id:nextId++,x:205,y:240,alt:10,hold:0},{id:nextId++,x:205,y:156,alt:10,hold:0},{id:nextId++,x:180,y:156,alt:10,hold:0},{id:nextId++,x:205,y:220,alt:10,hold:0}];points=points.slice(0,maxWaypoints);selected=points[0]?.id;
    drawCampus();renderPoints();message('Bản đồ campus đã sẵn sàng. Nhấn Kết nối để nhận dữ liệu thật.');
    const stream=new EventSource('/api/events');
    stream.onmessage=e=>{try{state=JSON.parse(e.data);backendAvailable=true;refresh();}catch(err){message('Dữ liệu trạng thái lỗi: '+err.message);}};
    stream.onerror=()=>{backendAvailable=false;refresh();message('Mất kết nối web backend; đang thử nối lại.');};
    setInterval(()=>{if(owner)api('/api/lease',{}).catch(e=>message(e.message));},4000);
  }catch(e){backendAvailable=false;message('Không khởi tạo được web: '+e.message);}
})();
