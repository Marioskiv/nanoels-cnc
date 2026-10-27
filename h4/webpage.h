// webpage.h - the web page served by the NanoEls H4 CNC fork (wifi.ino).
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later (this fork)
#pragma once
#include <pgmspace.h>

const char H4_WEB_PAGE[] PROGMEM = R"rawpage(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>NanoEls H4 CNC</title>
<style>
:root{--bg:#111;--fg:#eee;--dim:#999;--card:#1c1c1c;--line:#333;--acc:#3a8ee6;--ok:#2e9e5b;--warn:#d9a21b;--bad:#d64040}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:16px system-ui,sans-serif}
header{display:flex;gap:10px;align-items:center;padding:10px 14px;border-bottom:1px solid var(--line);flex-wrap:wrap}
h1{font-size:18px;margin:0 auto 0 0}.badge{padding:4px 10px;border-radius:12px;background:var(--line);font-weight:600}
main{display:grid;gap:12px;padding:12px;grid-template-columns:repeat(auto-fit,minmax(320px,1fr))}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px}
h2{font-size:15px;margin:0 0 8px;color:var(--dim);font-weight:600;text-transform:uppercase;letter-spacing:.05em}
.dro{display:grid;grid-template-columns:auto 1fr;gap:4px 12px;font:600 32px ui-monospace,monospace}
.dro span:nth-child(odd){color:var(--dim)}.small{font-size:13px;color:var(--dim)}
button,input[type=submit]{font:inherit;padding:10px 14px;border-radius:8px;border:1px solid var(--line);background:#2a2a2a;color:var(--fg);cursor:pointer}
button:active{filter:brightness(1.4)}button:disabled{opacity:.4}
.row{display:flex;gap:8px;flex-wrap:wrap;margin:6px 0}
.start{background:var(--ok)}.pause{background:var(--warn);color:#000}.stop{background:var(--bad);font-weight:700;flex:1 1 100%;font-size:22px;padding:16px}
.bar{height:14px;background:var(--line);border-radius:7px;overflow:hidden}.bar div{height:100%;background:var(--acc);width:0}
table{width:100%;border-collapse:collapse;font-size:14px}td{padding:6px 4px;border-bottom:1px solid var(--line)}
td:last-child{text-align:right;white-space:nowrap}td button{padding:6px 9px;font-size:14px}
canvas{width:100%;aspect-ratio:1;background:#000;border-radius:8px}
.keys{display:grid;grid-template-columns:repeat(5,1fr);gap:6px}.keys button{padding:12px 4px;font-size:14px}
input[type=text],input[type=password]{font:inherit;padding:8px;border-radius:6px;border:1px solid var(--line);background:#000;color:var(--fg);width:100%}
#msg{min-height:1.4em;color:var(--warn)}code{font-size:14px;word-break:break-all}
.lcd{background:#0b1f10;color:#7cf08f;font:600 17px ui-monospace,monospace;padding:10px;border-radius:8px;white-space:pre;overflow-x:auto;line-height:1.35}
.keys button.hold{touch-action:none;user-select:none;-webkit-user-select:none}.keys button.down{background:var(--acc)}
#log{font:13px ui-monospace,monospace;white-space:pre-wrap;max-height:220px;overflow:auto;color:var(--dim)}
details{border-top:1px solid var(--line);padding:6px 0}summary{cursor:pointer;font-weight:600;padding:4px 0}
.cfg{display:grid;grid-template-columns:1fr 150px;gap:6px 10px;align-items:center;font-size:14px;margin:6px 0}
.cfg input[type=number],.cfg select{font:inherit;padding:6px;border-radius:6px;border:1px solid var(--line);background:#000;color:var(--fg);width:100%}
.cfg .hint{grid-column:1/-1;font-size:12px;color:var(--dim);margin-top:-4px}.changed{outline:2px solid var(--warn)}
</style></head><body>
<header><h1>NanoEls H4 CNC</h1><span class="badge" id="state">-</span><span class="small" id="wifi"></span></header>
<main>
<section><h2>Position</h2>
<div class="dro"><span>X</span><span id="x">-</span><span>Z</span><span id="z">-</span><span id="cl">C</span><span id="c">-</span></div>
<div class="small" id="info"></div><div class="small" id="mach"></div>
<div class="row"><button id="bHome" onclick="if(confirm('Home All? The axes move to their switches.'))cmd('home')">Home All</button></div>
<div class="small">Inputs (terminal A2): <span id="ins"></span></div></section>

<section><h2>Program</h2>
<div><b id="pname">no program</b> <span class="small" id="stext"></span></div>
<div class="bar"><div id="pbar"></div></div>
<div class="small"><span id="ppct">0 %</span> &middot; <code id="pline"></code></div>
<div class="row"><button class="start" id="bStart" onclick="startSel()">Start</button>
<button class="pause" onclick="cmd('pause')">Pause</button><button onclick="cmd('resume')">Resume</button></div>
<div class="row"><button class="stop" onclick="cmd('stop')">STOP</button></div>
<form class="row" id="mdiform"><input type="text" id="mdi" placeholder="one G-code line, e.g. G0 C90" autocomplete="off" style="flex:1"><input type="submit" id="bMdi" value="Run line"></form>
<div class="small">Keep a real emergency stop within reach. The page is a convenience, not a safety device.</div>
<div id="msg"></div></section>

<section><h2>Path</h2><canvas id="cv" width="600" height="600"></canvas>
<div class="small" id="pathinfo">Select a file and press Draw.</div></section>

<section><h2>Files on the controller</h2>
<form id="up" class="row"><input type="file" id="file" accept=".nc,.ngc,.gcode,.tap,.txt"><input type="submit" value="Upload"></form>
<div class="bar"><div id="ubar"></div></div>
<table id="files"></table><div class="small" id="fs"></div></section>

<section><h2>H4 display and keys</h2><div class="lcd" id="lcd"></div><div style="height:8px"></div><div class="keys" id="keys"></div>
<div class="small">The same as the keys on the controller. Arrows, GEARS and TURN move while held; if the page loses
the connection the key is released within half a second.</div></section>

<section><h2>Gamepad</h2><div id="gpstate">No gamepad.</div>
<div class="small">PS4 / PS5 gamepad connected to this phone or computer by Bluetooth (or USB). Hold <b>L1</b> to move:
left stick Z, right stick X, L2 / R2 C+ / C-, R1 fast. L1 + Share = Home All, L1 + D-pad down = touch off Z,
Options = STOP, PS = unlock ($X). Keep this page open in front; it stops when the page is hidden or the WiFi drops.
Chrome on Android needs once: <code>chrome://flags</code> &rarr; <i>Insecure origins treated as secure</i> &rarr; add
<code id="gporigin"></code> &rarr; Enabled &rarr; Relaunch.</div>
<div class="cfg"><label for="gpN">Speed (mm/min)</label><input type="number" id="gpN" step="any">
<label for="gpF">Speed with R1 (mm/min)</label><input type="number" id="gpF" step="any">
<label for="gpC">C speed = speed x</label><input type="number" id="gpC" step="any">
<label for="gpIX">Reverse X</label><input type="checkbox" id="gpIX"><label for="gpIZ">Reverse Z</label><input type="checkbox" id="gpIZ">
<label for="gpIC">Reverse C</label><input type="checkbox" id="gpIC"></div></section>

<section><h2>Messages</h2><div id="log">No messages.</div></section>

<section><h2>Machine settings</h2><div id="cfg" class="small">Loading...</div>
<div class="row"><button onclick="saveCfg()">Save and restart</button><button onclick="resetCfg()">Defaults from the firmware</button></div>
<div class="small">Only while the machine is off. If the steps or the travel per turn of an axis (or the encoder) change,
the saved positions, stops and other H4 values are cleared.</div></section>

<section><h2>WiFi and firmware</h2>
<div class="row"><input type="text" id="ssid" placeholder="WiFi network name"><input type="password" id="pw" placeholder="password (empty = keep)">
<button onclick="saveWifi()">Save WiFi and restart</button></div>
<div class="row"><input type="file" id="fw" accept=".bin"><button onclick="uploadFw()">Update firmware (.bin)</button></div>
<div class="small" id="fwmsg"></div></section>
</main>
<script>
const $=id=>document.getElementById(id);let st={},selected='',drawn=null,files=[];
function msg(t){$('msg').textContent=t||''}
async function post(url,body){const r=await fetch(url,{method:'POST',body});const t=await r.text();if(!r.ok)throw new Error(t);return t}
async function cmd(c,extra){try{msg(await post('/cmd?do='+c+(extra||'')))}catch(e){msg(e.message)}}
function startSel(){if(!selected){msg('Select a file first.');return}
 if(confirm('Start '+selected+' on the machine?'))cmd('start','&name='+encodeURIComponent(selected))}
async function poll(){try{const r=await fetch('/status');st=await r.json();show()}catch(e){$('state').textContent='no connection'}
 setTimeout(poll,350)}
function show(){const u=st.inch?' in':' mm';$('state').textContent=st.state+(st.gcodeMode?'':' (not GCODE mode)');
 $('state').style.background={Run:'#2e9e5b',Hold:'#d9a21b',Paused:'#d9a21b',Stopping:'#d64040'}[st.state]||'#333';
 $('x').textContent=st.x.toFixed(3)+u;$('z').textContent=st.z.toFixed(3)+u;$('c').textContent=st.cActive?st.c.toFixed(3)+'°':'-';
 $('info').textContent='feed '+st.feed+' /min · spindle '+st.rpm+' rpm · step buffer empties: '+st.underruns+(st.estop?' · EMERGENCY STOP':'');
 $('wifi').textContent=st.wifi;$('pname').textContent=st.program||'no program';$('stext').textContent=st.text;
 const p=st.running||st.programLoaded?st.progress:0;$('pbar').style.width=(p*100)+'%';$('ppct').textContent=(p*100).toFixed(1)+' %';$('pline').textContent=st.line;
 $('bStart').disabled=!!st.running;$('fs').textContent=st.fsTotal?('free '+((st.fsTotal-st.fsUsed)/1048576).toFixed(2)+' MB of '+(st.fsTotal/1048576).toFixed(2)+' MB, program memory '+(st.psram?(st.psram/1048576).toFixed(2)+' MB (PSRAM)':'without PSRAM: up to 120 kB')):'';
 $('lcd').textContent=(st.lcd||[]).join('\n');
 const H=st.homed||[0,0,0];$('mach').textContent='Homed: X '+(H[0]?'yes':'no')+' · Z '+(H[1]?'yes':'no')+' · C '+(H[2]?'yes':'no')+
  (st.mx!==undefined?' · machine X '+st.mx.toFixed(3):'')+(st.mz!==undefined?' · machine Z '+st.mz.toFixed(3):'')+(st.homing?' · HOMING':'');
 $('bHome').disabled=!!st.running||!!st.homing;
 if(st.a2){const N=['A21 home X','A22 home Z','A23 alarm'],A=st.act||[0,0,0],U=st.use||[0,0,0];
  $('ins').textContent=N.map((n,i)=>n+': '+((st.in>>i)&1?'high':'LOW')+(U[i]?(A[i]?' (ACTIVE)':' (ok)'):'')).join(' · ')}
 else $('ins').textContent='used by handwheel 2';$('bMdi').disabled=!!st.running;
 if(st.log!==logSeen){logSeen=st.log;loadLog()}
 drawPath()}
async function listFiles(){try{const r=await fetch('/files');if(!r.ok){msg(await r.text());return}files=await r.json();
 $('files').innerHTML=files.map(f=>`<tr><td><label><input type="radio" name="sel" value="${f.name}" ${f.name==selected?'checked':''} onchange="selected=this.value"> ${f.name}</label></td><td>${(f.size/1024).toFixed(1)} kB</td><td><button onclick="draw('${f.name}')">Draw</button> <button onclick="del('${f.name}')">Delete</button></td></tr>`).join('')||'<tr><td>No files yet.</td></tr>'}catch(e){msg(e.message)}}
async function del(n){if(!confirm('Delete '+n+'?'))return;try{msg(await post('/delete?name='+encodeURIComponent(n)));if(selected==n)selected='';listFiles()}catch(e){msg(e.message)}}
$('up').onsubmit=e=>{e.preventDefault();const f=$('file').files[0];if(!f)return;const fd=new FormData();fd.append('file',f,f.name);
 const x=new XMLHttpRequest();x.open('POST','/upload');x.upload.onprogress=ev=>{$('ubar').style.width=(ev.loaded/ev.total*100)+'%'};
 x.onload=()=>{msg(x.responseText);$('ubar').style.width='0';selected=f.name;listFiles();f.text().then(t=>{drawn=parse(t,f.name)})};x.onerror=()=>msg('Upload failed');x.send(fd)};
async function draw(n){selected=n;listFiles();try{const r=await fetch('/file?name='+encodeURIComponent(n));if(!r.ok){msg(await r.text());return}drawn=parse(await r.text(),n)}catch(e){msg(e.message)}}
// Path: polar programs (with C) are drawn seen from the cutter (X radius, C angle); others as Z-X.
function parse(text,name){let abs=true,x=0,z=0,c=0,pts=[],hasC=false,chars=0;
 for(const raw of text.split('\n')){chars+=raw.length+1;const l=raw.replace(/\(.*?\)/g,'').replace(/;.*/,'').toUpperCase();
  if(/G91(?![\d.])/.test(l))abs=false;if(/G90(?![\d.])/.test(l))abs=true;
  const g=(k)=>{const m=l.match(new RegExp(k+'\\s*([-+]?[\\d.]+)'));return m?parseFloat(m[1]):null};
  const X=g('X'),Z=g('Z'),C=g('C');if(X===null&&Z===null&&C===null)continue;if(C!==null)hasC=true;
  if(X!==null)x=abs?X:x+X;if(Z!==null)z=abs?Z:z+Z;if(C!==null)c=abs?C:c+C;pts.push([x,z,c,chars,/G0(?![\d.])/.test(l)])}
 return{name,pts,hasC,total:chars}}
function xy(p,hasC){return hasC?[p[0]*Math.cos(p[2]*Math.PI/180),p[0]*Math.sin(p[2]*Math.PI/180)]:[p[1],p[0]]}
function drawPath(){const cv=$('cv'),g=cv.getContext('2d');g.clearRect(0,0,cv.width,cv.height);if(!drawn||!drawn.pts.length)return;
 const P=drawn.pts.map(p=>xy(p,drawn.hasC));let x0=1e9,x1=-1e9,y0=1e9,y1=-1e9;for(const[a,b]of P){x0=Math.min(x0,a);x1=Math.max(x1,a);y0=Math.min(y0,b);y1=Math.max(y1,b)}
 if(drawn.hasC){const r=Math.max(Math.abs(x0),Math.abs(x1),Math.abs(y0),Math.abs(y1));x0=y0=-r;x1=y1=r}
 const s=0.9*cv.width/Math.max(x1-x0,y1-y0,1e-3),ox=cv.width/2-s*(x0+x1)/2,oy=cv.height/2+s*(y0+y1)/2,T=([a,b])=>[ox+s*a,oy-s*b];
 const done=(st.program==drawn.name&&(st.running||st.programLoaded))?st.progress*drawn.total:0;
 g.lineWidth=1.2;for(let i=1;i<P.length;i++){const[a,b]=T(P[i-1]),[c2,d]=T(P[i]);g.strokeStyle=drawn.pts[i][3]<=done?'#3a8ee6':(drawn.pts[i][4]?'#553':'#888');g.beginPath();g.moveTo(a,b);g.lineTo(c2,d);g.stroke()}
 if(drawn.hasC){g.strokeStyle='#444';g.beginPath();g.arc(ox,oy,3,0,7);g.stroke()}
 const cur=T(xy([st.x,st.z,st.c],drawn.hasC));g.fillStyle='#e33';g.beginPath();g.arc(cur[0],cur[1],6,0,7);g.fill();
 $('pathinfo').textContent=drawn.name+': '+drawn.pts.length+' moves'+(drawn.hasC?', seen from the cutter (X radius, C angle)':', Z across, X up')}
let logSeen=-1,mdiBusy=false;
$('mdiform').onsubmit=async e=>{e.preventDefault();const l=$('mdi').value.trim();if(!l)return;
 try{msg(await post('/mdi',new URLSearchParams({line:l})))}catch(err){msg(err.message)}};
async function loadLog(){try{const r=await fetch('/log');const a=await r.json();$('log').textContent=a.length?a.join('\n'):'No messages.'}catch(e){}}
const HOLD=new Set([47,67,57,37,42,52]);let holdCode=0,holdTimer=0;
function holdStart(c,b){holdEnd();holdCode=c;b.classList.add('down');const send=()=>fetch('/key?code='+c+'&hold=1',{method:'POST'}).catch(()=>{});send();holdTimer=setInterval(send,150)}
function holdEnd(){if(!holdCode)return;clearInterval(holdTimer);fetch('/key?code='+holdCode+'&hold=0',{method:'POST'}).catch(()=>{});holdCode=0;
 document.querySelectorAll('.keys .down').forEach(b=>b.classList.remove('down'))}
addEventListener('blur',holdEnd);document.addEventListener('visibilitychange',holdEnd);
const KEYS=[['↑',47],['↓',67],['←',57],['→',37],['STEP',24],['-',5],['+',64],['ON',17],['OFF',27],['DISPL',14],
 ['GEARS',42],['TURN',52],['FACE',62],['CONE',3],['CUT',13],['THREAD',23],['MODE',33],['MEAS',54],['REV',44],['⌫',32],
 ['STOP L',7],['STOP R',15],['STOP U',6],['STOP D',16],['X0',53],['Z0',43],['A',4],['B',63],['7',11],['8',22],['9',1],['4',2],['5',21],['6',12],['1',41],['2',61],['3',31],['0',51]];
$('keys').innerHTML=KEYS.map(([t,c])=>HOLD.has(c)?`<button class="hold" data-c="${c}">${t}</button>`:`<button onclick="key(${c})">${t}</button>`).join('');
document.querySelectorAll('.keys .hold').forEach(b=>{const c=+b.dataset.c;
 b.addEventListener('pointerdown',e=>{e.preventDefault();b.setPointerCapture(e.pointerId);holdStart(c,b)});
 for(const ev of['pointerup','pointercancel','lostpointercapture'])b.addEventListener(ev,holdEnd);
 b.addEventListener('contextmenu',e=>e.preventDefault())});
// Machine settings: stored in the firmware's units (steps, deci-microns), shown in mm, mm/s, mm/s2.
let cfg=null;
function cfgVal(k){const it=cfg.items.find(i=>i.k==k);const el=$('c_'+k);return el?(it.t=='b'?(el.checked?1:0):el.value):it.v}
function spu(p){const steps=+cfgVal(p+'Steps');const it=cfg.items.find(i=>i.k==p+'Screw');const el=$('c_'+p+'Screw');
 const screw=el?+el.value*10000:it.v;return steps/(screw/10000)}
function unitText(it){const rot=it.k[0]=='a'&&cfg.items.find(i=>i.k=='aRot').v;const L=rot?'°':'mm';
 if(it.u=='du')return L;if(it.u.startsWith('v:'))return L+'/s';if(it.u.startsWith('a:'))return L+'/s²';return it.u}
function toShow(it,v){if(it.u=='du')return +(v/10000).toPrecision(8);if(it.u.startsWith('v:')||it.u.startsWith('a:'))return +(v/spu(it.u[2])).toPrecision(7);return v}
function renderCfg(){const groups={};for(const it of cfg.items)(groups[it.g]=groups[it.g]||[]).push(it);let h='';
 for(const g in groups){h+=`<details><summary>${g}</summary><div class="cfg">`;for(const it of groups[g]){const id='c_'+it.k,u=unitText(it);let inp;
  if(it.t=='b')inp=`<input type="checkbox" id="${id}" ${it.v?'checked':''}>`;
  else if(it.t=='a')inp=`<select id="${id}"><option ${it.v=='Z'?'selected':''}>Z</option><option ${it.v=='X'?'selected':''}>X</option></select>`;
  else inp=`<input type="number" step="any" id="${id}" value="${toShow(it,it.v)}">`;
  h+=`<label for="${id}">${it.l}${u?' ('+u+')':''}</label>${inp}`}
  h+='</div></details>'}
 $('cfg').innerHTML=h;$('cfg').classList.remove('small');
 $('cfg').querySelectorAll('input,select').forEach(el=>el.addEventListener('change',()=>el.classList.add('changed')))}
async function loadCfg(){try{const r=await fetch('/config');cfg=await r.json();renderCfg()}catch(e){$('cfg').textContent='Could not load the settings.'}}
async function saveCfg(){if(!cfg)return;const q=new URLSearchParams();
 for(const it of cfg.items){const el=$('c_'+it.k);let v;
  if(it.t=='b')v=el.checked?1:0;else if(it.t=='a')v=el.value;
  else{const x=parseFloat(el.value);if(!isFinite(x)){msg(it.g+', '+it.l+': not a number');return}
   if(it.u=='du')v=Math.round(x*10000);else if(it.u.startsWith('v:')||it.u.startsWith('a:'))v=Math.round(x*spu(it.u[2]));
   else if(it.t=='f')v=x;else v=Math.round(x)}
  q.append(it.k,v)}
 if(!confirm('Save the machine settings? The controller restarts.'))return;try{msg(await post('/config',q))}catch(e){msg(e.message)}}
async function resetCfg(){if(!confirm('Go back to the values written in the firmware? The controller restarts.'))return;
 try{msg(await post('/config?reset=1'))}catch(e){msg(e.message)}}
async function key(c){try{await post('/key?code='+c)}catch(e){msg(e.message)}}
async function saveWifi(){const fd=new URLSearchParams({ssid:$('ssid').value,password:$('pw').value});try{msg(await post('/wifi',fd))}catch(e){msg(e.message)}}
function uploadFw(){const f=$('fw').files[0];if(!f||!confirm('Update the firmware with '+f.name+'?'))return;const fd=new FormData();fd.append('fw',f,f.name);
 const x=new XMLHttpRequest();x.open('POST','/firmware');x.upload.onprogress=ev=>{$('fwmsg').textContent=Math.round(ev.loaded/ev.total*100)+' %'};
 x.onload=()=>{$('fwmsg').textContent=x.responseText};x.send(fd)}
// Gamepad: the layout of the LinuxCNC gamepad.hal. The page sends the wanted speeds to /jog every 100 ms while
// L1 is held; the H4 turns them into short jogs and stops by itself 0.3 s after the last request.
const GPD={n:400,f:1000,c:6,ix:false,iz:false,ic:false};let gp=Object.assign({},GPD);
try{Object.assign(gp,JSON.parse(localStorage.getItem('h4gamepad')||'{}'))}catch(e){}
const GPF={gpN:'n',gpF:'f',gpC:'c',gpIX:'ix',gpIZ:'iz',gpIC:'ic'};
for(const id in GPF){const el=$(id),k=GPF[id];if(el.type=='checkbox')el.checked=!!gp[k];else el.value=gp[k];
 el.addEventListener('change',()=>{gp[k]=el.type=='checkbox'?el.checked:(parseFloat(el.value)||GPD[k]);
  try{localStorage.setItem('h4gamepad',JSON.stringify(gp))}catch(e){}})}
$('gporigin').textContent=location.origin;
let gpPrev=[],gpLast=0,gpBusy=false,gpMoving=false,gpStops=0,gpErr='',gpPad=null;
function gpDz(v,d){const a=Math.abs(v);return a<=d?0:Math.sign(v)*(a-d)/(1-d)}
function gpRumble(){try{gpPad&&gpPad.vibrationActuator&&gpPad.vibrationActuator.playEffect('dual-rumble',{duration:200,strongMagnitude:.5,weakMagnitude:.5})}catch(e){}}
async function gpSend(x,z,c){gpLast=performance.now();gpBusy=true;
 try{const r=await fetch('/jog?x='+x.toFixed(1)+'&z='+z.toFixed(1)+'&c='+c.toFixed(1),{method:'POST'});
  if(!r.ok){const t=await r.text();if(t!=gpErr){gpErr=t;msg('Gamepad: '+t);gpRumble()}}else gpErr=''}catch(e){}
 gpBusy=false}
function gpMdi(l){post('/mdi',new URLSearchParams({line:l})).then(msg,e=>{msg(e.message);gpRumble()})}
function gpLoop(){let pads=[];
 try{pads=navigator.getGamepads?[...navigator.getGamepads()].filter(p=>p&&p.connected):[]}catch(e){}
 const g=pads[0];gpPad=g||null;const now=performance.now();
 if(!g||document.hidden){if(gpMoving||gpStops>0){if(now-gpLast>=100){gpSend(0,0,0);gpStops=gpMoving?3:gpStops-1;gpMoving=false}}
  $('gpstate').textContent=!window.isSecureContext&&!g?'No gamepad. Connect it, press a button. (Chrome: see the flag below.)':'No gamepad: connect it, then press a button.';return}
 const b=i=>!!(g.buttons[i]&&g.buttons[i].pressed),val=i=>g.buttons[i]?g.buttons[i].value:0,edge=i=>b(i)&&!gpPrev[i];
 const l1=b(4),sp=b(5)?gp.f:gp.n;
 const vz=(gp.iz?-1:1)*gpDz(g.axes[0]||0,.35)*sp,vx=(gp.ix?-1:1)*gpDz(g.axes[3]||0,.45)*sp;
 const vc=(gp.ic?-1:1)*(gpDz(val(6),.05)-gpDz(val(7),.05))*sp*gp.c;
 if(edge(9))cmd('stop');
 if(edge(16))gpMdi('$X');
 if(l1&&edge(8))cmd('home');
 if(l1&&edge(13))gpMdi('G10 L20 P0 Z0');
 gpPrev=g.buttons.map(x=>x.pressed);
 const want=l1&&(vx!=0||vz!=0||vc!=0);
 if(want){if(now-gpLast>=100&&!gpBusy){gpMoving=true;gpStops=3;gpSend(vx,vz,vc)}}
 else if(gpStops>0&&now-gpLast>=100){gpSend(0,0,0);gpStops=gpMoving?2:gpStops-1;gpMoving=false} // stop, sent 3 times
 $('gpstate').textContent=(g.mapping=='standard'?'':'(unknown button layout) ')+g.id+(want?' · moving':l1?' · L1 held':' · hold L1 to move')+(gpErr?' · '+gpErr:'')}
setInterval(gpLoop,20);
document.addEventListener('visibilitychange',()=>{if(document.hidden&&(gpMoving||gpStops))gpSend(0,0,0)});
fetch('/wifi').then(r=>r.json()).then(w=>{$('ssid').value=w.ssid}).catch(()=>{});
listFiles();loadCfg();poll();
</script></body></html>
)rawpage";
