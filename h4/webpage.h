// webpage.h - the web page served by the NanoEls H4 CNC fork (wifi.ino).
// https://github.com/Marioskiv/nanoels
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
</style></head><body>
<header><h1>NanoEls H4 CNC</h1><span class="badge" id="state">-</span><span class="small" id="wifi"></span></header>
<main>
<section><h2>Position</h2>
<div class="dro"><span>X</span><span id="x">-</span><span>Z</span><span id="z">-</span><span id="cl">C</span><span id="c">-</span></div>
<div class="small" id="info"></div></section>

<section><h2>Program</h2>
<div><b id="pname">no program</b> <span class="small" id="stext"></span></div>
<div class="bar"><div id="pbar"></div></div>
<div class="small"><span id="ppct">0 %</span> &middot; <code id="pline"></code></div>
<div class="row"><button class="start" id="bStart" onclick="startSel()">Start</button>
<button class="pause" onclick="cmd('pause')">Pause</button><button onclick="cmd('resume')">Resume</button></div>
<div class="row"><button class="stop" onclick="cmd('stop')">STOP</button></div>
<div class="small">Keep a real emergency stop within reach. The page is a convenience, not a safety device.</div>
<div id="msg"></div></section>

<section><h2>Path</h2><canvas id="cv" width="600" height="600"></canvas>
<div class="small" id="pathinfo">Select a file and press Draw.</div></section>

<section><h2>Files on the controller</h2>
<form id="up" class="row"><input type="file" id="file" accept=".nc,.ngc,.gcode,.tap,.txt"><input type="submit" value="Upload"></form>
<div class="bar"><div id="ubar"></div></div>
<table id="files"></table><div class="small" id="fs"></div></section>

<section><h2>H4 keys</h2><div class="keys" id="keys"></div>
<div class="small">The same as the keys on the controller (one press each).</div></section>

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
const KEYS=[['↑',47],['↓',67],['←',57],['→',37],['STEP',24],['-',5],['+',64],['ON',17],['OFF',27],['DISPL',14],
 ['GEARS',42],['TURN',52],['FACE',62],['CONE',3],['CUT',13],['THREAD',23],['MODE',33],['MEAS',54],['REV',44],['⌫',32],
 ['STOP L',7],['STOP R',15],['STOP U',6],['STOP D',16],['X0',53],['Z0',43],['A',4],['B',63],['7',11],['8',22],['9',1],['4',2],['5',21],['6',12],['1',41],['2',61],['3',31],['0',51]];
$('keys').innerHTML=KEYS.map(([t,c])=>`<button onclick="key(${c})">${t}</button>`).join('');
async function key(c){try{await post('/key?code='+c)}catch(e){msg(e.message)}}
async function saveWifi(){const fd=new URLSearchParams({ssid:$('ssid').value,password:$('pw').value});try{msg(await post('/wifi',fd))}catch(e){msg(e.message)}}
function uploadFw(){const f=$('fw').files[0];if(!f||!confirm('Update the firmware with '+f.name+'?'))return;const fd=new FormData();fd.append('fw',f,f.name);
 const x=new XMLHttpRequest();x.open('POST','/firmware');x.upload.onprogress=ev=>{$('fwmsg').textContent=Math.round(ev.loaded/ev.total*100)+' %'};
 x.onload=()=>{$('fwmsg').textContent=x.responseText};x.send(fd)}
fetch('/wifi').then(r=>r.json()).then(w=>{$('ssid').value=w.ssid}).catch(()=>{});
listFiles();poll();
</script></body></html>
)rawpage";
