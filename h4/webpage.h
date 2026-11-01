// webpage.h - the small emergency page served by the NanoEls H4 CNC fork (wifi.ino). The full screen lives in
// the Android app (android/app/src/main/assets/h4/index.html), which serves it for the H4's address.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later (this fork)
#pragma once
#include <pgmspace.h>

const char H4_WEB_PAGE[] PROGMEM = R"rawpage(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>NanoEls H4 CNC</title>
<style>
:root{--bg:#111;--fg:#eee;--dim:#999;--card:#1c1c1c;--line:#333;--bad:#d64040;--ok:#2e9e5b}
@media (prefers-color-scheme:light){:root{--bg:#f2f3f5;--fg:#15181d;--dim:#5b6470;--card:#fff;--line:#cfd5dd}}
body{margin:0;background:var(--bg);color:var(--fg);font:15px system-ui,sans-serif}
main{max-width:560px;margin:0 auto;padding:12px 16px;display:flex;flex-direction:column;gap:12px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px;display:flex;flex-direction:column;gap:8px}
h1{font-size:18px;margin:0}h2{font-size:15px;margin:0}.dim{color:var(--dim);font-size:13px}
button,input{font:inherit;padding:9px 12px;border-radius:8px;border:1px solid var(--line);background:var(--bg);color:var(--fg)}
#stop{background:var(--bad);color:#fff;font-size:22px;font-weight:800;padding:18px;border:0}
.pos{font:600 20px ui-monospace,monospace}.bad{color:var(--bad);font-weight:700}a{color:inherit}
</style></head><body><main>
<h1>NanoEls H4 CNC</h1>
<div class="dim">This is the H4's small emergency page. The full screen (control, lathe, G-code, settings) is in the
<b>H4 CNC app</b> for Android: <a href="https://github.com/Marioskiv/nanoels-cnc/releases/latest">download it</a>.
Η πλήρης οθόνη είναι στην εφαρμογή H4 CNC (Android).</div>
<button id="stop" onclick="cmd('stop')">STOP</button>
<div class="card"><h2>Status</h2><div id="st" class="pos">-</div><div id="info" class="dim"></div>
<div id="rs" class="bad" hidden></div><button id="rsb" hidden onclick="cmd('confirmRestart')">Zeros checked</button></div>
<div class="card"><h2>WiFi</h2><input id="ssid" placeholder="Network name"><input id="pw" type="password" placeholder="Password">
<button onclick="wifi()">Save and restart</button></div>
<div class="card"><h2>Firmware</h2><input id="fw" type="file" accept=".bin"><button onclick="fw()">Update</button><div id="fwm" class="dim"></div>
<div class="dim">Do not switch the H4 off in the first 20 s after an update.</div></div>
<div class="card"><h2>Lasting log</h2><a href="/events">Open the log</a></div>
<div id="msg" class="dim"></div>
</main><script>
const $=i=>document.getElementById(i),msg=t=>$('msg').textContent=t;
async function post(u,b){const r=await fetch(u,{method:'POST',body:b});const t=await r.text();if(!r.ok)throw new Error(t);return t}
async function cmd(d){try{msg(await post('/cmd?do='+d))}catch(e){msg(e.message)}}
async function wifi(){try{msg(await post('/wifi',new URLSearchParams({ssid:$('ssid').value,password:$('pw').value})))}catch(e){msg(e.message)}}
function fw(){const f=$('fw').files[0];if(!f||!confirm('Update the firmware with '+f.name+'?'))return;const d=new FormData();d.append('fw',f,f.name);
 const x=new XMLHttpRequest();x.open('POST','/firmware');x.upload.onprogress=e=>{$('fwm').textContent=Math.round(e.loaded/e.total*100)+' %'};
 x.onload=()=>{$('fwm').textContent=x.responseText};x.send(d)}
async function poll(){try{const s=await (await fetch('/status')).json(),u=s.inch?' in':' mm';
 $('st').textContent='X '+s.x.toFixed(3)+'  Z '+s.z.toFixed(3)+(s.cActive?'  C '+s.c.toFixed(2)+'°':'');
 $('info').textContent=s.state+' · mode '+s.mode+' · firmware '+s.fw+(s.estop?' · EMERGENCY STOP':'')+(s.text?' · '+s.text:'');
 $('rs').hidden=$('rsb').hidden=!s.restart;$('rs').textContent=s.restart||''}catch(e){$('info').textContent='No connection'}}
setInterval(poll,1000);poll();fetch('/wifi').then(r=>r.json()).then(w=>{$('ssid').value=w.ssid}).catch(()=>{});
</script></body></html>)rawpage";
