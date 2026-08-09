#ifndef PAGE_H
#define PAGE_H

static const char PAGE_HTML[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Smart Bell</title>
<style>
:root{
  --bg:#14161a; --panel:#1c1f26; --panel2:#232730; --border:#2c3038;
  --text:#e8e9ec; --dim:#9aa0ac; --accent:#7c5cff; --accent2:#6a4ce8;
  --green:#3ddc84; --blue:#4da6ff; --red:#ff5c5c; --amber:#ffb74d;
  --radius:12px;
}
*{box-sizing:border-box;}
body{background:var(--bg); color:var(--text); font-family:-apple-system,'Segoe UI',Roboto,sans-serif; margin:0; padding:16px;}
.wrap{max-width:820px; margin:0 auto;}
.header{display:flex; justify-content:space-between; align-items:flex-start; flex-wrap:wrap; gap:12px; margin-bottom:14px;}
.header h1{margin:0; font-size:20px; font-weight:700;}
.header h2{margin:2px 0 0; font-size:13px; font-weight:400; color:var(--dim);}
.status{text-align:right;}
.clock{font-size:24px; font-family:'SF Mono',Consolas,monospace; font-weight:600; letter-spacing:1px;}
.clock-src{font-size:11px; color:var(--dim); margin-top:2px;}
.btn{border:none; padding:10px 18px; border-radius:20px; color:#fff; cursor:pointer; font-weight:600; font-size:14px; display:inline-flex; align-items:center; gap:6px;}
.btn-accent{background:var(--accent);}
.btn-accent:hover{background:var(--accent2);}
.btn-ghost{background:var(--panel2); color:var(--text); border:1px solid var(--border);}
.btn-ghost:hover{background:#2a2e37;}
.btn-danger-outline{background:transparent; color:var(--red); border:1px solid var(--red); padding:6px 12px; font-size:12px; border-radius:8px;}
.btn-danger-outline:hover{background:rgba(255,92,92,.1);}
.ring-btn{margin-top:8px;}
.statusbar{display:flex; gap:14px; flex-wrap:wrap; align-items:center; background:var(--panel); border:1px solid var(--border); border-radius:var(--radius); padding:10px 14px; font-size:12px; color:var(--dim); margin-bottom:16px;}
.dot{width:8px; height:8px; border-radius:50%; display:inline-block; margin-right:5px;}
.dot-green{background:var(--green);} .dot-red{background:var(--red);} .dot-blue{background:var(--blue);} .dot-amber{background:var(--amber);}
.next-badge{background:linear-gradient(135deg,var(--accent),var(--accent2)); color:#fff; border-radius:var(--radius); padding:10px 14px; font-size:13px; margin-bottom:16px; display:none;}
.next-badge b{font-size:15px;}
.card{background:var(--panel); border:1px solid var(--border); border-radius:var(--radius); padding:16px; margin-bottom:16px;}
.card h3{margin:0 0 12px; font-size:14px; color:var(--dim); text-transform:uppercase; letter-spacing:.5px; font-weight:600;}
.day-selector{display:flex; gap:6px; flex-wrap:wrap; margin-bottom:14px;}
.day-checkbox{display:none;}
.day-label{padding:8px 12px; background:var(--panel2); border:1px solid var(--border); border-radius:8px; cursor:pointer; user-select:none; font-size:13px; font-weight:600; color:var(--dim);}
.day-checkbox:checked + .day-label{background:var(--accent); color:#fff; border-color:var(--accent2);}
.form-row{display:flex; gap:12px; flex-wrap:wrap; align-items:end;}
.field{display:flex; flex-direction:column; gap:4px;}
.field label{font-size:11px; color:var(--dim); font-weight:600;}
input[type=number], input[type=text], select{background:var(--panel2); border:1px solid var(--border); color:var(--text); padding:8px 10px; font-size:14px; border-radius:8px;}
input[type=number]{width:64px;}
input#labelInput{width:160px;}
select{cursor:pointer;}
.time-group{display:flex; align-items:center; gap:4px;}
.form-actions{margin-left:auto; display:flex; gap:8px;}
.event-list{background:var(--panel); border:1px solid var(--border); border-radius:var(--radius); padding:8px; }
.list-header{display:flex; justify-content:space-between; align-items:center; padding:8px 8px 4px;}
.list-header b{font-size:14px;}
.day-group{margin:10px 6px;}
.day-group-title{font-size:12px; font-weight:700; color:var(--dim); text-transform:uppercase; letter-spacing:.5px; padding:6px 4px;}
.event-item{display:flex; align-items:center; gap:10px; padding:10px 8px; border-bottom:1px solid var(--border); flex-wrap:wrap;}
.event-item:last-child{border-bottom:none;}
.event-time{font-weight:700; font-size:15px; width:88px;}
.event-label{color:var(--dim); font-size:13px; flex:1; min-width:80px;}
.event-dur{color:var(--dim); font-size:12px; background:var(--panel2); padding:2px 8px; border-radius:10px;}
.event-item.disabled .event-time, .event-item.disabled .event-label{opacity:.4;}
.toggle{position:relative; width:38px; height:22px; flex-shrink:0;}
.toggle input{opacity:0; width:0; height:0;}
.slider{position:absolute; inset:0; background:var(--panel2); border:1px solid var(--border); border-radius:22px; cursor:pointer; transition:.15s;}
.slider:before{content:""; position:absolute; height:16px; width:16px; left:2px; top:2px; background:var(--dim); border-radius:50%; transition:.15s;}
.toggle input:checked + .slider{background:var(--accent);}
.toggle input:checked + .slider:before{transform:translateX(16px); background:#fff;}
.icon-btn{background:none; border:none; color:var(--dim); cursor:pointer; font-size:16px; padding:4px 6px; border-radius:6px;}
.icon-btn:hover{background:var(--panel2); color:var(--text);}
.icon-btn.danger:hover{color:var(--red);}
.empty-state{text-align:center; color:var(--dim); padding:30px 10px; font-size:14px;}
.toast{position:fixed; bottom:20px; left:50%; transform:translateX(-50%); background:var(--panel2); border:1px solid var(--border); color:var(--text); padding:10px 18px; border-radius:10px; font-size:13px; opacity:0; pointer-events:none; transition:.25s; z-index:50;}
.toast.show{opacity:1; transform:translateX(-50%) translateY(-4px);}
.toast.error{border-color:var(--red); color:var(--red);}
.toast.ok{border-color:var(--green); color:var(--green);}
footer{text-align:center; color:var(--dim); font-size:11px; margin-top:20px; padding-bottom:10px;}
@media(max-width:520px){
  .event-label{order:5; width:100%; margin-left:98px;}
  .status{text-align:left;}
  .header{flex-direction:column;}
}
</style>
</head><body>
<div class="wrap">

  <div class="header">
    <div>
      <h1>Nasir Higher Secondary School</h1>
      <h2>Smart Bell System &mdash; Rabwah</h2>
    </div>
    <div class="status">
      <div class="clock" id="clock">--:--:--</div>
      <div class="clock-src" id="clockSrc">Loading&hellip;</div>
      <div style="margin-top:8px; display:flex; gap:8px; justify-content:flex-end;">
        <button class="btn btn-accent ring-btn" onclick="ringNow()">&#128276; Ring Now (5s)</button>
        <a href="/logout" class="btn btn-ghost ring-btn" style="text-decoration:none;">Logout</a>
      </div>
    </div>
  </div>

  <div class="statusbar" id="statusbar">
    <span><span class="dot dot-red" id="wifiDot"></span><span id="wifiText">WiFi</span></span>
    <span><span class="dot dot-red" id="rtcDot"></span><span id="rtcText">RTC</span></span>
    <span id="eventCountText">0 events</span>
    <span id="heapText"></span>
  </div>

  <div class="next-badge" id="nextBadge"></div>

  <div class="card">
    <h3 id="formTitle">Add Bell</h3>
    <div class="day-selector" id="daySelector">
      <input type="checkbox" id="d1" class="day-checkbox" checked><label for="d1" class="day-label">Mon</label>
      <input type="checkbox" id="d2" class="day-checkbox" checked><label for="d2" class="day-label">Tue</label>
      <input type="checkbox" id="d3" class="day-checkbox" checked><label for="d3" class="day-label">Wed</label>
      <input type="checkbox" id="d4" class="day-checkbox" checked><label for="d4" class="day-label">Thu</label>
      <input type="checkbox" id="d5" class="day-checkbox"><label for="d5" class="day-label">Fri</label>
      <input type="checkbox" id="d6" class="day-checkbox"><label for="d6" class="day-label">Sat</label>
      <input type="checkbox" id="d0" class="day-checkbox" checked><label for="d0" class="day-label">Sun</label>
    </div>
    <div class="form-row">
      <div class="field">
        <label>Time</label>
        <div class="time-group">
          <input type="number" id="hourInput" min="1" max="12" value="8">
          <span>:</span>
          <input type="number" id="minuteInput" min="0" max="59" value="0">
          <select id="ampmInput"><option value="0">AM</option><option value="1">PM</option></select>
        </div>
      </div>
      <div class="field">
        <label>Duration (s)</label>
        <input type="number" id="durInput" min="1" max="15" value="3">
      </div>
      <div class="field">
        <label>Label (optional)</label>
        <input type="text" id="labelInput" maxlength="24" placeholder="e.g. Period 1">
      </div>
      <div class="form-actions">
        <button class="btn btn-ghost" id="cancelEditBtn" style="display:none" onclick="cancelEdit()">Cancel</button>
        <button class="btn btn-accent" id="submitBtn" onclick="submitForm()">Add to Schedule</button>
      </div>
    </div>
  </div>

  <div class="event-list">
    <div class="list-header">
      <b>Scheduled Bells</b>
      <button class="btn-danger-outline" onclick="clearAll()">Delete All</button>
    </div>
    <div id="eventListBody"></div>
  </div>

  <div class="card" style="margin-top:16px;">
    <h3>Account</h3>
    <div class="form-row">
      <div class="field">
        <label>Current Password</label>
        <input type="password" id="currentPwInput" autocomplete="current-password">
      </div>
      <div class="field">
        <label>New Password</label>
        <input type="password" id="newPwInput" autocomplete="new-password">
      </div>
      <div class="form-actions">
        <button class="btn btn-ghost" onclick="changePassword()">Change Password</button>
      </div>
    </div>
  </div>

  <footer>Smart Bell v2 &middot; ESP8266 + DS3231</footer>
</div>

<div class="toast" id="toast"></div>

<script>
const DAY_NAMES = ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"];
const DAY_ORDER = [1,2,3,4,5,6,0]; // display order: Mon..Sun
let events = [];
let status = {valid:false};
let editingId = null;

function toast(msg, kind){
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.className = 'toast show' + (kind ? ' ' + kind : '');
  clearTimeout(t._timer);
  t._timer = setTimeout(()=> t.className='toast', 2200);
}

async function api(path, opts){
  try{
    const res = await fetch(path, opts);
    if(res.status === 401){ window.location.href = '/login'; return; }
    const data = await res.json().catch(()=>({ok:res.ok}));
    if(!res.ok || data.ok === false) throw new Error(data.message || 'Request failed');
    return data;
  }catch(e){
    toast(e.message || 'Network error', 'error');
    throw e;
  }
}

function fmtTime12(h,m){
  const pm = h >= 12;
  let h12 = h % 12; if(h12===0) h12 = 12;
  return h12 + ':' + String(m).padStart(2,'0') + ' ' + (pm?'PM':'AM');
}

function renderClock(){
  if(!status.valid){ document.getElementById('clock').textContent = '--:--:--'; return; }
  const h = status.hour, m = status.minute, s = status.second;
  document.getElementById('clock').textContent = fmtTime12(h,m) + ':' + String(s).padStart(2,'0').slice(-2);
  // recompute below with seconds included properly
  const pm = h >= 12; let h12 = h % 12; if(h12===0) h12 = 12;
  document.getElementById('clock').textContent =
    String(h12).padStart(2,'0') + ':' + String(m).padStart(2,'0') + ':' + String(s).padStart(2,'0') + ' ' + (pm?'PM':'AM');
}

function renderStatusBar(){
  const wifiDot = document.getElementById('wifiDot');
  const rtcDot = document.getElementById('rtcDot');
  wifiDot.className = 'dot ' + (status.wifi ? 'dot-green' : 'dot-red');
  document.getElementById('wifiText').textContent = status.wifi ? 'WiFi connected' : 'WiFi offline';
  rtcDot.className = 'dot ' + (status.rtc ? (status.ntpSynced ? 'dot-blue' : 'dot-amber') : 'dot-red');
  document.getElementById('rtcText').textContent = status.rtc ? 'RTC OK' : 'RTC not found';
  document.getElementById('eventCountText').textContent = events.length + ' event' + (events.length===1?'':'s');
  document.getElementById('heapText').textContent = status.freeHeap ? ('heap ' + Math.round(status.freeHeap/1024) + 'KB') : '';

  const srcEl = document.getElementById('clockSrc');
  srcEl.textContent = 'Source: ' + (status.source || '...');
  srcEl.style.color = status.source==='NTP' ? '#3ddc84' : (status.source==='RTC' ? '#4da6ff' : '#ff5c5c');
}

function computeNextBell(){
  if(!status.valid) return null;
  const nowMin = status.hour*60 + status.minute;
  let best = null, bestDelta = Infinity;
  for(const e of events){
    if(!e.enabled) continue;
    for(let offset=0; offset<7; offset++){
      const day = (status.weekday + offset) % 7;
      if(day !== e.day) continue;
      const evMin = e.hour*60 + e.minute;
      let delta;
      if(offset===0){
        if(evMin <= nowMin) continue;
        delta = evMin - nowMin;
      } else {
        delta = offset*1440 - nowMin + evMin;
      }
      if(delta < bestDelta){ bestDelta = delta; best = e; }
    }
  }
  return best ? {event:best, minutesAway:bestDelta} : null;
}

function renderNextBadge(){
  const badge = document.getElementById('nextBadge');
  const next = computeNextBell();
  if(!next){ badge.style.display = 'none'; return; }
  const e = next.event;
  const hrs = Math.floor(next.minutesAway/60), mins = next.minutesAway%60;
  const when = hrs>0 ? (hrs+'h '+mins+'m') : (mins+'m');
  badge.style.display = 'block';
  badge.innerHTML = 'Next bell in <b>' + when + '</b> &mdash; ' + DAY_NAMES[e.day] + ' ' + fmtTime12(e.hour,e.minute) +
    (e.label ? ' &middot; ' + e.label : '');
}

function renderEventList(){
  const body = document.getElementById('eventListBody');
  if(events.length === 0){
    body.innerHTML = '<div class="empty-state">No bells scheduled yet. Add one above.</div>';
    return;
  }
  let html = '';
  for(const day of DAY_ORDER){
    const dayEvents = events.filter(e=>e.day===day).sort((a,b)=> (a.hour*60+a.minute)-(b.hour*60+b.minute));
    if(dayEvents.length === 0) continue;
    html += '<div class="day-group"><div class="day-group-title">' + DAY_NAMES[day] + '</div>';
    for(const e of dayEvents){
      html += '<div class="event-item' + (e.enabled?'':' disabled') + '">' +
        '<span class="event-time">' + fmtTime12(e.hour,e.minute) + '</span>' +
        '<span class="event-label">' + (e.label ? escapeHtml(e.label) : '') + '</span>' +
        '<span class="event-dur">' + e.duration + 's</span>' +
        '<label class="toggle"><input type="checkbox" ' + (e.enabled?'checked':'') + ' onchange="toggleEvent(' + e.id + ')"><span class="slider"></span></label>' +
        '<button class="icon-btn" onclick="editEvent(' + e.id + ')" title="Edit">&#9998;</button>' +
        '<button class="icon-btn danger" onclick="deleteEvent(' + e.id + ')" title="Delete">&#10006;</button>' +
        '</div>';
    }
    html += '</div>';
  }
  body.innerHTML = html;
}

function escapeHtml(s){
  return s.replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}

function renderAll(){ renderClock(); renderStatusBar(); renderNextBadge(); renderEventList(); }

async function fetchStatus(){
  try{
    const res = await fetch('/api/status');
    if(res.status === 401){ window.location.href = '/login'; return; }
    status = await res.json();
    renderClock(); renderStatusBar(); renderNextBadge();
  }catch(e){ /* keep last known status, network hiccup */ }
}

async function fetchEvents(){
  try{
    const res = await fetch('/api/events');
    if(res.status === 401){ window.location.href = '/login'; return; }
    events = await res.json();
    renderEventList(); renderNextBadge(); renderStatusBar();
  }catch(e){ /* keep last known list */ }
}

function tickClock(){
  if(!status.valid) return;
  status.second++;
  if(status.second >= 60){ status.second=0; status.minute++; }
  if(status.minute >= 60){ status.minute=0; status.hour++; }
  if(status.hour >= 24){ status.hour=0; status.weekday=(status.weekday+1)%7; }
  renderClock();
}

function collectSelectedDays(){
  const days = [];
  for(let d=0; d<=6; d++){ if(document.getElementById('d'+d).checked) days.push(d); }
  return days;
}

function to24(hour12, ampm){
  let h = hour12 % 12;
  if(ampm === 1) h += 12;
  return h;
}

async function submitForm(){
  const days = collectSelectedDays();
  const hour = parseInt(document.getElementById('hourInput').value || '8', 10);
  const minute = parseInt(document.getElementById('minuteInput').value || '0', 10);
  const ampm = parseInt(document.getElementById('ampmInput').value, 10);
  const dur = parseInt(document.getElementById('durInput').value || '3', 10);
  const label = document.getElementById('labelInput').value.trim();

  if(editingId !== null){
    const body = new URLSearchParams({id:editingId, day:days[0] ?? 0, hour, minute, ampm, dur, label});
    await api('/api/events/update', {method:'POST', body});
    toast('Bell updated', 'ok');
    cancelEdit();
  } else {
    if(days.length === 0){ toast('Select at least one day', 'error'); return; }
    const body = new URLSearchParams({hour, minute, ampm, dur, label});
    days.forEach(d => body.append('d'+d, '1'));
    await api('/api/events/add', {method:'POST', body});
    toast('Bell added', 'ok');
  }
  await fetchEvents();
}

function editEvent(id){
  const e = events.find(x=>x.id===id);
  if(!e) return;
  editingId = id;
  document.getElementById('formTitle').textContent = 'Edit Bell';
  document.getElementById('submitBtn').textContent = 'Save Changes';
  document.getElementById('cancelEditBtn').style.display = 'inline-flex';
  for(let d=0; d<=6; d++) document.getElementById('d'+d).checked = (d === e.day);
  const pm = e.hour >= 12; let h12 = e.hour % 12; if(h12===0) h12 = 12;
  document.getElementById('hourInput').value = h12;
  document.getElementById('minuteInput').value = e.minute;
  document.getElementById('ampmInput').value = pm ? 1 : 0;
  document.getElementById('durInput').value = e.duration;
  document.getElementById('labelInput').value = e.label || '';
  window.scrollTo({top:0, behavior:'smooth'});
}

function cancelEdit(){
  editingId = null;
  document.getElementById('formTitle').textContent = 'Add Bell';
  document.getElementById('submitBtn').textContent = 'Add to Schedule';
  document.getElementById('cancelEditBtn').style.display = 'none';
  document.getElementById('labelInput').value = '';
}

async function toggleEvent(id){
  await api('/api/events/toggle', {method:'POST', body:new URLSearchParams({id})});
  await fetchEvents();
}

async function deleteEvent(id){
  await api('/api/events/delete', {method:'POST', body:new URLSearchParams({id})});
  toast('Bell deleted', 'ok');
  await fetchEvents();
}

async function clearAll(){
  if(!confirm('Delete ALL scheduled bells? This cannot be undone.')) return;
  await api('/api/events/clear', {method:'POST'});
  toast('All bells deleted', 'ok');
  await fetchEvents();
}

async function ringNow(){
  await api('/api/ring', {method:'POST'});
  toast('Ringing for 5s', 'ok');
}

async function changePassword(){
  const current = document.getElementById('currentPwInput').value;
  const newpass = document.getElementById('newPwInput').value;
  if(!current || !newpass){ toast('Fill in both fields', 'error'); return; }
  await api('/api/account/password', {method:'POST', body:new URLSearchParams({current, newpass})});
  toast('Password changed', 'ok');
  document.getElementById('currentPwInput').value = '';
  document.getElementById('newPwInput').value = '';
}

// Poll status every 10s (server does the local-time conversion; we
// just tick seconds locally in between to keep the clock smooth
// without hitting the device every second from every open browser tab).
fetchStatus();
fetchEvents();
setInterval(fetchStatus, 10000);
setInterval(fetchEvents, 30000);
setInterval(tickClock, 1000);
</script>
</body></html>
)HTMLPAGE";

#endif // PAGE_H
