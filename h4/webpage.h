// webpage.h - the web page served by the NanoEls H4 CNC fork (wifi.ino).
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later (this fork)
#pragma once
#include <pgmspace.h>

const char H4_WEB_PAGE[] PROGMEM = R"rawpage(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>NanoEls H4 CNC</title>
<style>
:root{--bg:#111;--fg:#eee;--dim:#999;--card:#1c1c1c;--line:#333;--acc:#3a8ee6;--ok:#2e9e5b;--warn:#d9a21b;--bad:#d64040;--or:#ff9a1f;--btn:#2a2a2a;--in:#000;--ctl1:#151a24;--ctl2:#0d1016;--ctlb:#262d3a;--tile:#12161f;--tileb:#232a37;--btn2:#141922;--w1:#202734;--w2:#0e1219;--wb:#2a3240;--track:#273041;--dro:34px}
body.light{--bg:#eef0f3;--fg:#15181d;--dim:#5b6470;--card:#fff;--line:#cfd5dd;--btn:#e7eaef;--in:#fff;--ctl1:#ffffff;--ctl2:#eef2f7;--ctlb:#cfd5dd;--tile:#f6f8fb;--tileb:#d6dce4;--btn2:#e9edf2;--w1:#ffffff;--w2:#e3e8ef;--wb:#cfd5dd;--track:#d6dce4}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:16px system-ui,sans-serif}
html,body{overflow-x:hidden}
header{position:sticky;top:0;z-index:5;background:var(--bg);display:flex;gap:10px;align-items:center;padding:8px 12px;padding-top:max(8px,env(safe-area-inset-top));border-bottom:1px solid var(--line)}
.hstop{padding:6px 14px;font-weight:800;letter-spacing:.05em;background:var(--bad);color:#fff;border-color:var(--bad)}
h1{font-size:17px;margin:0 auto 0 0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;min-width:0}.badge{padding:4px 10px;border-radius:12px;background:var(--line);font-weight:600}
main{display:grid;gap:12px;padding:12px;padding-bottom:calc(84px + env(safe-area-inset-bottom));grid-template-columns:repeat(auto-fit,minmax(min(340px,100%),1fr));align-items:start}
main>section{min-width:0}main>section[hidden]{display:none}
/* tab bar (as in phone apps): one screen per tab, no long scrolling */
.tabs{position:fixed;left:0;right:0;bottom:0;z-index:5;display:grid;grid-template-columns:repeat(5,1fr);background:var(--card);border-top:1px solid var(--line);padding-bottom:env(safe-area-inset-bottom)}
.tabs button{position:relative;border:0;border-radius:0;background:none;padding:7px 2px 6px;font-size:11px;color:var(--dim);display:flex;flex-direction:column;align-items:center;gap:2px}
.tabs svg{width:24px;height:24px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.tabs button.on{color:var(--or)}.tabs button.on::before{content:'';position:absolute;top:0;left:22%;right:22%;height:3px;border-radius:0 0 3px 3px;background:var(--or)}
.tabs .dot{position:absolute;top:4px;left:calc(50% + 8px);min-width:9px;height:9px;border-radius:5px;background:var(--bad)}.tabs .pct{position:absolute;top:3px;left:calc(50% + 10px);font:700 10px ui-monospace,monospace;color:var(--ok)}
.toast{position:fixed;left:12px;right:12px;bottom:calc(70px + env(safe-area-inset-bottom));z-index:6;background:#222;color:#fff;border:1px solid var(--line);border-radius:10px;padding:10px 12px;font-size:14px;box-shadow:0 6px 20px #0008;transition:opacity .3s;max-width:560px;margin:auto}
.toast.off{opacity:0;pointer-events:none}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px}
h2{font-size:15px;margin:0 0 8px;color:var(--dim);font-weight:600;text-transform:uppercase;letter-spacing:.05em}
.dro{display:grid;grid-template-columns:auto 1fr;gap:4px 12px;font:600 32px ui-monospace,monospace}
.dro span:nth-child(odd){color:var(--dim)}.small{font-size:13px;color:var(--dim)}
button,input[type=submit]{font:inherit;padding:10px 14px;border-radius:8px;border:1px solid var(--line);background:var(--btn);color:var(--fg);cursor:pointer}
button:active{filter:brightness(1.4)}button:disabled{opacity:.4}
.row{display:flex;gap:8px;flex-wrap:wrap;margin:6px 0}
.start{background:var(--ok)}.pause{background:var(--warn);color:#000}.stop{background:var(--bad);font-weight:700;flex:1 1 100%;font-size:22px;padding:16px}
.bar{height:14px;background:var(--line);border-radius:7px;overflow:hidden}.bar div{height:100%;background:var(--acc);width:0}
table{width:100%;border-collapse:collapse;font-size:14px}td{padding:6px 4px;border-bottom:1px solid var(--line)}
td{word-break:break-word}td:last-child{text-align:right;white-space:nowrap}td button{padding:6px 9px;font-size:14px}
canvas{width:100%;aspect-ratio:1;background:#000;border-radius:8px}
.keys{display:grid;grid-template-columns:repeat(5,1fr);gap:6px}.keys button{padding:12px 4px;font-size:14px}
input[type=text],input[type=password]{font:inherit;padding:8px;border-radius:6px;border:1px solid var(--line);background:var(--in);color:var(--fg);width:100%}
#msg{min-height:1.4em;color:var(--warn)}code{font-size:14px;word-break:break-all}
.lcd{background:#0b1f10;color:#7cf08f;font:600 17px ui-monospace,monospace;padding:10px;border-radius:8px;white-space:pre;overflow-x:auto;line-height:1.35}
.keys button.hold{touch-action:none;user-select:none;-webkit-user-select:none}.keys button.down{background:var(--acc)}
#log{font:13px ui-monospace,monospace;white-space:pre-wrap;max-height:220px;overflow:auto;color:var(--dim)}
details{border-top:1px solid var(--line);padding:6px 0}summary{cursor:pointer;font-weight:600;padding:4px 0}
.cfg{display:grid;grid-template-columns:minmax(0,1fr) min(150px,42%);gap:6px 10px;align-items:center;font-size:14px;margin:6px 0}
.cfg input[type=number],.cfg select{font:inherit;padding:6px;border-radius:6px;border:1px solid var(--line);background:var(--in);color:var(--fg);width:100%}
.cfg input[type=color]{width:100%;height:36px;padding:2px;border-radius:6px;border:1px solid var(--line);background:var(--in)}
.cfg .hint{grid-column:1/-1;font-size:12px;color:var(--dim);margin-top:-4px}.changed{outline:2px solid var(--warn)}
/* control screen */
.ctl{grid-column:1/-1;background:linear-gradient(180deg,var(--ctl1),var(--ctl2));border-color:var(--ctlb)}
.ctop{display:flex;gap:14px;align-items:center;flex-wrap:wrap;font-size:13px;color:var(--dim);margin-bottom:10px}.ctop b{color:var(--fg)}
.pill{padding:4px 12px;border-radius:14px;border:1px solid #2f6db3;color:#9ecbff;font-weight:700;letter-spacing:.05em}
.grow{flex:1}.mini{padding:6px 12px;font-size:13px;font-weight:700;letter-spacing:.05em;background:var(--btn2)}
.mini.go{color:#36d399}.mini.hold{color:var(--warn)}.mini.halt{color:#ff5a5a;border-color:#5a2020}
.cgrid{display:grid;gap:14px;grid-template-columns:minmax(0,1fr)}.cgrid>div{min-width:0}@media(min-width:760px){.cgrid{grid-template-columns:minmax(0,1fr) minmax(0,1fr)}}
.axis{display:grid;grid-template-columns:30px minmax(0,1fr) auto;align-items:center;gap:8px;padding:8px 10px;margin-bottom:8px;border-radius:12px;
 background:var(--tile);border:1px solid var(--tileb);border-left:5px solid var(--c);cursor:pointer}
.axis.sel{border-color:var(--c);box-shadow:0 0 14px -4px var(--c)}
.axis .l{font:800 28px system-ui;color:var(--c)}.axis .v{font:600 min(var(--dro),8vw) ui-monospace,monospace;text-align:right;white-space:nowrap;overflow:hidden;text-overflow:clip}
.axis .v small{font-size:14px;color:var(--dim);margin-left:4px}.axis .m{grid-column:2;font:12px ui-monospace,monospace;color:var(--dim);text-align:right;margin-top:-6px}
.axis .z{grid-row:1/3;grid-column:3;font-size:11px;padding:8px 8px;line-height:1.1}
.chips{display:grid;grid-template-columns:repeat(6,1fr);gap:4px;margin:6px 0 10px}.chips button{min-width:0;padding:8px 0;font:600 14px ui-monospace,monospace}
.chips button.on{background:var(--or);color:#000;border-color:var(--or)}
.wheelrow{display:flex;align-items:center;justify-content:center;gap:14px}
.round{width:56px;height:56px;border-radius:50%;font-size:26px;padding:0;background:var(--btn2)}
.wheel{position:relative;width:min(56vw,230px);aspect-ratio:1;touch-action:none;user-select:none;-webkit-user-select:none}
.wheel svg{width:100%;height:100%}.wcenter{position:absolute;inset:26%;border-radius:50%;background:radial-gradient(var(--w1),var(--w2));
 display:flex;flex-direction:column;align-items:center;justify-content:center;border:1px solid var(--wb);pointer-events:none}
.wcenter b{font:800 40px system-ui;color:var(--or)}.wcenter .small{font:11px system-ui;white-space:nowrap}.wcenter span{font:13px ui-monospace,monospace;color:var(--fg)}
.ovr{display:grid;grid-template-columns:52px 38px minmax(0,1fr) 46px 38px;align-items:center;gap:8px;margin-top:10px;font-size:12px;color:var(--dim)}
.ovr button{padding:6px 0}.obar{height:6px;background:var(--track);border-radius:3px;overflow:hidden}.obar div{height:100%;background:var(--or);width:50%}
.ovr b{color:var(--fg);font:600 14px ui-monospace,monospace;text-align:right}
.cnav{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin-top:12px}.cnav button{min-width:0;padding:12px 2px;font-size:14px;background:var(--btn2)}
.cnav button.on{border-color:var(--or);box-shadow:inset 0 -3px 0 var(--or)}
.macros{display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));gap:6px;margin-top:10px}
</style></head><body>
<header><h1 id="title">NanoEls H4 CNC</h1><span class="badge" id="state">-</span><button class="hstop" onclick="cmd('stop')">STOP</button></header>
<main>
<section class="ctl" data-tab="control"><div class="ctop"><span class="pill" id="cState">-</span><span>WCS <b id="cWcs">G54</b></span><span>Units <b id="cUnits">mm</b></span>
<span id="cRssi"></span><span class="grow"></span><button class="mini go" onclick="startSel()">START</button><button class="mini hold" id="cHold" onclick="cmd(st.state=='Hold'?'resume':'pause')">HOLD</button></div>
<div class="cgrid"><div id="axes"></div>
<div><div class="small">STEP PER DETENT <b id="stepTxt"></b></div><div class="chips" id="chips"></div>
<div class="wheelrow"><button class="round" id="jm">&minus;</button><div class="wheel" id="wheel"><svg viewBox="0 0 200 200"><circle cx="100" cy="100" r="92" fill="var(--w2)" stroke="var(--wb)"/><g id="ticks"></g>
<path d="M100 14 A86 86 0 0 1 172 55" fill="none" stroke="var(--or)" stroke-width="4" stroke-linecap="round"/></svg>
<div class="wcenter"><b id="wAxis">X</b><span id="wStep"></span><span class="small">turn to jog</span></div></div><button class="round" id="jp">+</button></div>
<div class="ovr"><span>FEED</span><button onclick="ovr(146)">&minus;</button><div class="obar"><div id="foBar"></div></div><b id="foTxt">100%</b><button onclick="ovr(145)">+</button></div>
<div class="ovr"><span>RAPIDS</span><button onclick="ovr(st.ro>50?150:151)">&minus;</button><div class="obar"><div id="roBar"></div></div><b id="roTxt">100%</b><button onclick="ovr(st.ro<50?150:149)">+</button></div></div></div>
<nav class="cnav"><button onclick="homeAll()">Home</button><button onclick="go('fileSec')">Files</button><button id="bMac" onclick="toggleMacros()">Macros</button>
<button onclick="go('mdi')">MDI</button></nav><div class="macros" id="macros" hidden></div></section>
<section data-tab="machine"><h2>Position</h2>
<div class="dro"><span>X</span><span id="x">-</span><span>Z</span><span id="z">-</span><span id="cl">C</span><span id="c">-</span></div>
<div class="small" id="info"></div><div class="small" id="mach"></div>
<div class="row"><button id="bHome" onclick="if(confirm('Home All? The axes move to their switches.'))cmd('home')">Home All</button></div>
<div class="small">Inputs (terminal A2): <span id="ins"></span></div></section>

<section data-tab="program"><h2>Program</h2>
<div><b id="pname">no program</b> <span class="small" id="stext"></span></div>
<div class="bar"><div id="pbar"></div></div>
<div class="small"><span id="ppct">0 %</span> &middot; <code id="pline"></code></div>
<div class="row"><button class="start" id="bStart" onclick="startSel()">Start</button>
<button class="pause" onclick="cmd('pause')">Pause</button><button onclick="cmd('resume')">Resume</button></div>
<div class="row"><button class="stop" onclick="cmd('stop')">STOP</button></div>
<div class="row"><span>Feed <b id="fo">100</b> %</span><button onclick="ovr(146)">&minus;10</button><button onclick="ovr(144)">100 %</button><button onclick="ovr(145)">+10</button>
<span>Rapids <b id="ro">100</b> %</span><button onclick="ovr(151)">25</button><button onclick="ovr(150)">50</button><button onclick="ovr(149)">100</button></div>
<form class="row" id="mdiform"><input type="text" id="mdi" placeholder="one G-code line, e.g. G0 C90" autocomplete="off" style="flex:1"><input type="submit" id="bMdi" value="Run line"></form>
<div class="small">Keep a real emergency stop within reach. The page is a convenience, not a safety device.</div>
<div id="msg"></div></section>

<section data-tab="program" style="order:1"><h2>Path</h2><canvas id="cv" width="600" height="600"></canvas>
<div class="small" id="pathinfo">Select a file and press Draw.</div></section>

<section id="fileSec" data-tab="program"><h2>Files on the controller</h2>
<form id="up" class="row"><input type="file" id="file" accept=".nc,.ngc,.gcode,.tap,.txt"><input type="submit" value="Upload"></form>
<div class="bar"><div id="ubar"></div></div>
<table id="files"></table><div class="small" id="fs"></div></section>

<section data-tab="h4"><h2>H4 display and keys</h2><div class="lcd" id="lcd"></div><div style="height:8px"></div><div class="keys" id="keys"></div>
<div class="small">The same as the keys on the controller. Arrows, GEARS and TURN move while held; if the page loses
the connection the key is released within half a second.</div></section>

<section data-tab="h4"><h2>Gamepad</h2><div id="gpstate">No gamepad.</div>
<div class="small">PS4 / PS5 gamepad connected to this phone or computer by Bluetooth (or USB). Hold <b>L1</b> to move:
left stick Z, right stick X, L2 / R2 C+ / C-, R1 fast. L1 + Share = Home All, L1 + D-pad down = touch off Z,
Options = STOP, PS = unlock ($X). D-pad (without L1): up / down feed +10 / &minus;10 %, left 100 %. Keep this page open in front; it stops when the page is hidden or the WiFi drops.
<span id="gpflag">Best with the H4 CNC Android app. In Chrome on Android instead, once: <code>chrome://flags</code> &rarr;
<i>Insecure origins treated as secure</i> &rarr; add <code id="gporigin"></code> &rarr; Enabled &rarr; Relaunch.</span></div>
<div class="row" id="gpapp" style="display:none"><button onclick="H4App.changeAddress()">H4 address</button></div>
<div class="cfg"><label for="gpN">Speed (mm/min)</label><input type="number" id="gpN" step="any">
<label for="gpF">Speed with R1 (mm/min)</label><input type="number" id="gpF" step="any">
<label for="gpC">C speed = speed x</label><input type="number" id="gpC" step="any">
<label for="gpIX">Reverse X</label><input type="checkbox" id="gpIX"><label for="gpIZ">Reverse Z</label><input type="checkbox" id="gpIZ">
<label for="gpIC">Reverse C</label><input type="checkbox" id="gpIC"></div></section>

<section data-tab="machine"><h2>Diagnostics</h2><div id="diag" class="small">-</div>
<div class="row"><button onclick="cmd('diagReset')">Clear</button></div>
<div class="small">Measured by the controller itself since the program started (or Clear): the step interrupt must
stay well below the time between two steps at the highest step rate, it should never start late, and the step buffer
should never run empty. Run a program in the air at full speed with WiFi in use to see the real limits.</div></section>

<section id="logSec" data-tab="machine"><h2>Messages</h2><div id="log">No messages.</div></section>

<section id="lookSec" data-tab="settings"><h2>Appearance</h2><div class="cfg">
<label for="lkTheme">Theme</label><select id="lkTheme"><option value="dark">Dark</option><option value="light">Light</option></select>
<label for="lkAcc">Accent colour</label><input type="color" id="lkAcc">
<label for="lkX">X colour</label><input type="color" id="lkX"><label for="lkZ">Z colour</label><input type="color" id="lkZ">
<label for="lkC">C colour</label><input type="color" id="lkC">
<label for="lkSize">Number size</label><select id="lkSize"><option value="34">Normal</option><option value="44">Large</option><option value="56">Very large</option></select></div>
<div class="row"><button onclick="lookReset()">Back to the original colours</button></div>
<div class="small">Kept in this phone or browser only; the machine is not affected.</div></section>

<section id="cfgSec" data-tab="settings"><h2>Machine settings</h2><div id="cfg" class="small">Loading...</div>
<div class="row"><button onclick="saveCfg()">Save and restart</button><button onclick="resetCfg()">Defaults from the firmware</button></div>
<div class="small">Only while the machine is off. If the steps or the travel per turn of an axis (or the encoder) change,
the saved positions, stops and other H4 values are cleared.</div></section>

<section data-tab="settings"><h2>WiFi and firmware</h2>
<div class="row"><input type="text" id="ssid" placeholder="WiFi network name"><input type="password" id="pw" placeholder="password (empty = keep)">
<button onclick="saveWifi()">Save WiFi and restart</button></div>
<div class="row"><input type="file" id="fw" accept=".bin"><button onclick="uploadFw()">Update firmware (.bin)</button></div>
<div class="row" id="fwApp" style="display:none"><button onclick="H4App.checkFirmware()">Check for a firmware update (from GitHub)</button></div>
<div class="small">Firmware <b id="fwVer">-</b>. The settings of the machine stay through an update.</div>
<div class="small" id="wifi"></div><div class="small" id="fwmsg"></div></section>
</main>
<nav class="tabs" id="tabs"><button data-t="control" onclick="tab('control')"><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="2.5"/><path d="M12 4v3M20 12h-3M12 20v-3M4 12h3"/></svg><span>Control</span></button><button data-t="program" onclick="tab('program')"><svg viewBox="0 0 24 24"><path d="M6 3h8l4 4v14H6z"/><path d="M14 3v4h4M9 12h6M9 16h6"/></svg><span>Program</span></button><button data-t="h4" onclick="tab('h4')"><svg viewBox="0 0 24 24"><rect x="3" y="6" width="18" height="12" rx="3"/><path d="M7 12h4M9 10v4"/><circle cx="15.5" cy="11" r="1"/><circle cx="17.5" cy="13.5" r="1"/></svg><span>H4 keys</span></button><button data-t="machine" onclick="tab('machine')"><svg viewBox="0 0 24 24"><path d="M3 12h4l3-7 4 14 3-7h4"/></svg><span>Machine</span></button><button data-t="settings" onclick="tab('settings')"><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M4.9 19.1L7 17M17 7l2.1-2.1"/></svg><span>Settings</span></button></nav><div class="toast off" id="toast"></div>
<script>
// Demo (the page opened with ?demo, e.g. in the Android app without an H4): a simulated machine answers instead
// of the controller, so the page can be seen and tried. Nothing here runs when the page comes from an H4.
const DEMO=/[?&]demo\b/.test(location.search);
if(DEMO)(function(){
 const S={x:20,z:5,c:0,fo:100,ro:100,state:'Idle',running:false,paused:false,progress:0,homed:[0,0,0],jog:{x:0,z:0,c:0,t:0},
  log:['Demo: a simulated machine, no H4 is connected.'],t:Date.now(),benchN:0,bench:'',program:'',steps:0,
  files:[{name:'demo_spiral.ngc'},{name:'demo_F_test.ngc'}]};
 const lines=['(demo program)','G21 G90 G94 G8','G0 Z5','G0 X40 C0','G1 Z-0.5 F200','G93'];
 for(let i=0;i<=720;i+=6)lines.push('G1 X'+(40-i/24).toFixed(3)+' C'+i+' F'+(60).toFixed(1));
 lines.push('G94','G0 Z5','M2');const text={'demo_spiral.ngc':lines.join('\n'),
  'demo_F_test.ngc':'G21 G90 G94\nG0 Z5\nG0 X10 C0\nG1 Z-0.3 F100\nG1 X30 F300\nG1 C20\nG1 X20 C20\nG1 X20 C35\nG0 Z5\nM2'};
 for(const f of S.files)f.size=text[f.name].length;
 const pts=lines.map(l=>/X([-\d.]+) C([-\d.]+)/.exec(l)).filter(Boolean).map(m=>[+m[1],+m[2]]);
 const add=t=>S.log.push(t);
 function tick(){const now=Date.now(),dt=Math.min(0.5,(now-S.t)/1000);S.t=now;
  if(now-S.jog.t<300&&!S.running){S.x+=S.jog.x/60*dt;S.z+=S.jog.z/60*dt;S.c+=S.jog.c/60*dt;S.steps+=Math.round((Math.abs(S.jog.x)+Math.abs(S.jog.z))*20*dt)}
  if(S.running&&!S.paused){S.progress=Math.min(1,S.progress+dt/40*S.fo/100);const p=pts[Math.min(pts.length-1,Math.floor(S.progress*pts.length))];
   S.x=p[0];S.c=p[1];S.z=-0.5;S.steps+=Math.round(400*dt*S.fo/100);
   if(S.progress>=1){S.running=false;S.state='Idle';S.z=5;add('Demo: program end')}}}
 function status(){tick();const st=S.running?(S.paused?'Hold':'Run'):'Idle';
  return {state:st,gcodeMode:1,text:S.paused?'Paused':'',x:S.x,z:S.z,c:S.c,cActive:1,inch:0,feed:S.running?60*S.fo/100:0,rpm:0,
   line:S.running?lines[Math.min(lines.length-1,6+Math.floor(S.progress*pts.length))]:'',progress:S.progress,program:S.program,
   programLoaded:S.program?1:0,underruns:0,estop:0,running:S.running?1:0,fsTotal:10e6,fsUsed:4200,psram:2e6,heap:150000,
   wifi:'DEMO - no H4 connected, nothing moves',lcd:['DEMO','X '+S.x.toFixed(3),'Z '+S.z.toFixed(3),'C '+S.c.toFixed(2)],
   homed:S.homed,mx:S.x-24.039,mz:S.z-10,homing:0,a2:1,in:7,act:[0,0,0],use:[1,1,0],log:S.log.length,
   diag:{isrUs:3.2,lateUs:0.6,late:0,rate:S.running?24000:0,limit:100000,steps:S.steps},benchN:S.benchN,bench:S.bench,fo:S.fo,ro:S.ro,wcs:'G54',rssi:-52,fw:'demo'}}
 const cfgItems=[{k:'xSteps',t:'l',v:2400,g:'X axis',l:'Motor steps per screw turn',u:'steps'},{k:'xScrew',t:'l',v:20000,g:'X axis',l:'Travel per screw turn',u:'du'},
  {k:'xSpeed',t:'l',v:14400,g:'X axis',l:'Maximum speed (manual moves, G0)',u:'v:x'},{k:'zSteps',t:'l',v:800,g:'Z axis',l:'Motor steps per screw turn',u:'steps'},
  {k:'zScrew',t:'l',v:20000,g:'Z axis',l:'Travel per screw turn',u:'du'},{k:'zSpeed',t:'l',v:10000,g:'Z axis',l:'Maximum speed (manual moves, G0)',u:'v:z'},
  {k:'aAct',t:'b',v:1,g:'A1 axis (C)',l:'Connected (terminal A1)',u:''},{k:'aRot',t:'b',v:1,g:'A1 axis (C)',l:'Rotary (degrees)',u:''},
  {k:'aSteps',t:'l',v:1376,g:'A1 axis (C)',l:'Motor steps per screw turn',u:'steps'},{k:'aScrew',t:'l',v:150000,g:'A1 axis (C)',l:'Travel per screw turn',u:'du'},
  {k:'aSpeed',t:'l',v:33024,g:'A1 axis (C)',l:'Maximum speed (manual moves, G0)',u:'v:a'}];
 function handle(url,opt){const u=new URL(url,'http://demo/'),q=u.searchParams,p=u.pathname,b=opt&&opt.body;
  const arg=k=>q.get(k)!==null?q.get(k):(b instanceof URLSearchParams?b.get(k):null);
  const ok=t=>[200,typeof t=='string'?t:JSON.stringify(t)],no=t=>[409,t];
  if(p=='/status')return ok(status());
  if(p=='/log')return ok(S.log.slice(-40));
  if(p=='/files')return ok(S.files);
  if(p=='/file')return text[arg('name')]!==undefined?ok(text[arg('name')]):[404,'File not found'];
  if(p=='/config')return opt&&opt.method=='POST'?ok('Demo: settings are not saved.'):ok({items:cfgItems});
  if(p=='/wifi')return opt&&opt.method=='POST'?ok('Demo: not saved.'):ok({ssid:'demo',passwordSet:0,setupAp:0,ip:'demo',apName:'NanoEls-H4-Setup'});
  if(p=='/jog'){S.jog={x:+arg('x')||0,z:+arg('z')||0,c:+arg('c')||0,t:Date.now()};return ok('ok')}
  if(p=='/key')return ok('ok');
  if(p=='/delete'){S.files=S.files.filter(f=>f.name!=arg('name'));return ok('Deleted')}
  if(p=='/mdi'){const l=(arg('line')||'').trim();if(S.running)return no('The machine is running a program. Stop it first.');
   if(/^\$H$/i.test(l)){S.homed=[1,1,1];S.x=0;S.z=0;add('Demo: Home All');return ok('Home All')}
   const m=/G91 G1 ([XZC])([-\d.]+)/i.exec(l);if(m){S[m[1].toLowerCase()]+= +m[2]}
   add('Demo: '+l);return ok('Running: '+l)}
  if(p=='/cmd'){const d=arg('do');
   if(d=='start'){if(S.running)return no('The machine is running a program. Stop it first.');S.program=arg('name');S.running=true;S.paused=false;S.progress=0;add('Demo: start '+S.program);return ok('Started '+S.program)}
   if(d=='stop'){S.running=false;S.paused=false;add('Demo: STOP');return ok('Stopping')}
   if(d=='pause'){if(!S.running)return no('Nothing runs.');S.paused=true;return ok('Pausing')}
   if(d=='resume'){S.paused=false;return ok('Resuming')}
   if(d=='home'){S.homed=[1,1,1];S.x=0;S.z=0;add('Demo: Home All');return ok('Home All')}
   if(d=='ovr'){const c=+arg('code');if(c==144)S.fo=100;if(c==145)S.fo+=10;if(c==146)S.fo-=10;if(c==147)S.fo++;if(c==148)S.fo--;
    if(c==149)S.ro=100;if(c==150)S.ro=50;if(c==151)S.ro=25;S.fo=Math.max(10,Math.min(200,S.fo));return ok('Feed '+S.fo+' %, rapids '+S.ro+' %')}
   if(d=='diagReset'){S.steps=0;return ok('Diagnostics cleared')}
   if(d=='bench'){const n=arg('name'),L=(text[n]||'').split('\n').length;S.benchN++;
    S.bench='Test '+n+' (demo): '+L+' lines. The H4 plans 6000 moves/s, the program needs at most 25/s: OK, 240 times faster than needed';add(S.bench);return ok('Testing '+n+' (nothing moves) ...')}
   return [400,'Unknown command']}
  return ok('Demo: '+p)}
 window.fetch=async(url,opt)=>{const [code,body]=handle(url,opt||{});return new Response(body,{status:code})};
 window.h4DemoRequest=url=>{handle(url,{method:'POST'})};
 window.XMLHttpRequest=function(){const x=this;x.upload={};x.open=()=>{};x.send=()=>setTimeout(()=>{x.responseText='Demo: files are not stored.';x.onload&&x.onload()},100)};
})();
const $=id=>document.getElementById(id);let st={},selected='',drawn=null,files=[];
let toastT=0;function msg(t){$('msg').textContent=t||'';if(!t)return;const e=$('toast');e.textContent=t;e.classList.remove('off');clearTimeout(toastT);toastT=setTimeout(()=>e.classList.add('off'),4000)}
async function post(url,body){const r=await fetch(url,{method:'POST',body});const t=await r.text();if(!r.ok)throw new Error(t);return t}
async function cmd(c,extra){try{msg(await post('/cmd?do='+c+(extra||'')))}catch(e){msg(e.message)}}
function startSel(){if(!selected){msg('Select a file first.');return}
 if(confirm('Start '+selected+' on the machine?'))cmd('start','&name='+encodeURIComponent(selected))}
let polls=0;async function poll(){try{const r=await fetch('/status');st=await r.json();polls++;show()}catch(e){$('state').textContent='no connection'}
 setTimeout(poll,350)}
function ovr(c){cmd('ovr','&code='+c)}
let benchSeen=-1;function bench(n){cmd('bench','&name='+encodeURIComponent(n))}
function diagShow(d){if(!d)return;const per=d.rate?1e6/d.rate:0;
 const ok=d.late==0&&(!d.rate||d.isrUs<0.7*per)&&!st.underruns;
 $('diag').innerHTML=(ok?'<b>OK</b>':'<b style="color:#d64040">CHECK</b>')+' · step interrupt longest '+d.isrUs+' µs'+(d.rate?' (time between steps at the highest rate: '+per.toFixed(1)+' µs)':'')+
  ' · highest step rate '+d.rate+' /s (limit '+d.limit+' /s) · started late (over 10 µs): '+d.late+' times, latest '+d.lateUs+' µs · step buffer ran empty: '+st.underruns+' · steps given: '+d.steps}
function show(){const u=st.inch?' in':' mm';diagShow(st.diag);ctlShow();if(st.fw)$('fwVer').textContent=st.fw;
 if(st.benchN!==undefined){if(benchSeen>=0&&st.benchN!=benchSeen)msg(st.bench);benchSeen=st.benchN}if(st.fo!==undefined){$('fo').textContent=st.fo;$('ro').textContent=st.ro}$('state').textContent=st.state+(st.gcodeMode?'':' (not GCODE mode)');
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
 if(st.log!==logSeen){logSeen=st.log;loadLog();if(logRead<0||curTab=='machine')logRead=st.log}tabBadges();
 drawPath()}
async function listFiles(){try{const r=await fetch('/files');if(!r.ok){msg(await r.text());return}files=await r.json();
 $('files').innerHTML=files.map(f=>`<tr><td><label><input type="radio" name="sel" value="${f.name}" ${f.name==selected?'checked':''} onchange="selected=this.value"> ${f.name}</label></td><td>${(f.size/1024).toFixed(1)} kB</td><td><button onclick="draw('${f.name}')">Draw</button> <button onclick="bench('${f.name}')" title="Reads and plans the file as when it runs, nothing moves: is the controller fast enough for it?">Test</button> <button onclick="del('${f.name}')">Delete</button></td></tr>`).join('')||'<tr><td>No files yet.</td></tr>'}catch(e){msg(e.message)}}
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
  try{localStorage.setItem('h4gamepad',JSON.stringify(gp))}catch(e){}gpToApp()})}
// In the H4 CNC Android app the app reads the gamepad itself (no browser setting needed); the page only shows its state.
const gpApp=!!window.H4App;function gpToApp(){if(gpApp)try{H4App.settings(JSON.stringify(gp))}catch(e){}}
if(gpApp){$('fwApp').style.display='';$('gpflag').style.display='none';$('gpapp').style.display='';window.h4AppGamepad=t=>{$('gpstate').textContent=t};gpToApp()}
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
 if(!l1&&edge(12))ovr(145);if(!l1&&edge(13))ovr(146);if(!l1&&edge(14))ovr(144); // feed override +10 / -10 / 100 %
 gpPrev=g.buttons.map(x=>x.pressed);
 const want=l1&&(vx!=0||vz!=0||vc!=0);
 if(want){if(now-gpLast>=100&&!gpBusy){gpMoving=true;gpStops=3;gpSend(vx,vz,vc)}}
 else if(gpStops>0&&now-gpLast>=100){gpSend(0,0,0);gpStops=gpMoving?2:gpStops-1;gpMoving=false} // stop, sent 3 times
 $('gpstate').textContent=(g.mapping=='standard'?'':'(unknown button layout) ')+g.id+(want?' · moving':l1?' · L1 held':' · hold L1 to move')+(gpErr?' · '+gpErr:'')}
if(!gpApp)setInterval(gpLoop,20);
document.addEventListener('visibilitychange',()=>{if(document.hidden&&(gpMoving||gpStops))gpSend(0,0,0)});
fetch('/wifi').then(r=>r.json()).then(w=>{$('ssid').value=w.ssid}).catch(()=>{});
// Control screen: DRO with work and machine position, ZERO per axis, a jog wheel (one step per detent), overrides, macros.
const STEPS=[0.001,0.01,0.1,1,5,10];let jogAxis='X',jogStep=0.1,jogPending=0,jogBusy=false,jogPoll=-9,wheelAngle=0;
try{jogStep=+localStorage.getItem('h4step')||0.1}catch(e){}
const LOOK0={theme:'dark',acc:'#ff9a1f',X:'#ff9a1f',Z:'#7ad151',C:'#e24bd0',size:'34'};let look=Object.assign({},LOOK0);
try{Object.assign(look,JSON.parse(localStorage.getItem('h4look')||'{}'))}catch(e){}
const AXC={X:look.X,Z:look.Z,C:look.C};
// Appearance: theme, accent and axis colours, number size (stored in the browser)
function lookApply(){document.body.classList.toggle('light',look.theme=='light');const r=document.documentElement.style;
 r.setProperty('--or',look.acc);r.setProperty('--dro',look.size+'px');AXC.X=look.X;AXC.Z=look.Z;AXC.C=look.C;
 const a=$('axes');if(a)a.dataset.n='';if(typeof ctlJogShow=='function'&&$('chips'))ctlJogShow()}
function lookSave(){try{localStorage.setItem('h4look',JSON.stringify(look))}catch(e){}lookApply()}
function lookReset(){look=Object.assign({},LOOK0);lookForm();lookSave()}
function lookForm(){$('lkTheme').value=look.theme;$('lkAcc').value=look.acc;$('lkX').value=look.X;$('lkZ').value=look.Z;$('lkC').value=look.C;$('lkSize').value=look.size}
lookForm();for(const [id,k] of [['lkTheme','theme'],['lkAcc','acc'],['lkX','X'],['lkZ','Z'],['lkC','C'],['lkSize','size']])
 $(id).addEventListener('input',()=>{look[k]=$(id).value;lookSave()});
lookApply();
function axesShow(){const u=st.inch?'in':'mm',list=['X','Z'].concat(st.cActive?['C']:[]);
 const val={X:st.x,Z:st.z,C:st.c},mach={X:st.mx,Z:st.mz,C:undefined};
 if($('axes').dataset.n!=list.join()){$('axes').dataset.n=list.join();$('axes').innerHTML=list.map(a=>`<div class="axis" id="ax${a}" style="--c:${AXC[a]}" onclick="selAxis('${a}')"><span class="l">${a}</span>
  <span class="v"><span id="v${a}"></span><small>${a=='C'?'°':u}</small></span><button class="z" onclick="event.stopPropagation();zeroAxis('${a}')">Ø<br>ZERO</button><span class="m" id="m${a}"></span></div>`).join('')}
 for(const a of list){$('v'+a).textContent=(val[a]||0).toFixed(3);$('m'+a).textContent=mach[a]!==undefined?'MCS '+mach[a].toFixed(3):'';$('ax'+a).classList.toggle('sel',a==jogAxis)}}
function selAxis(a){jogAxis=a;ctlJogShow();axesShow()}
function zeroAxis(a){const warn=a=='X'?'\n\nOn a polar machine X0 must stay the chuck centre: zero X only if the tool is exactly there.':'';
 if(confirm('Set '+a+' = 0 here (active work offset)?'+warn))gpMdi('G10 L20 P0 '+a+'0')}
function homeAll(){if(confirm('Home All? The axes move to their switches.'))cmd('home')}
function go(id){const e=$(id);if(!e)return;const s=e.closest('section');if(s&&s.dataset.tab)tab(s.dataset.tab);
 setTimeout(()=>{e.scrollIntoView({behavior:'smooth',block:'start'});if(id=='mdi')$('mdi').focus()},50)}
// Tabs: one screen each (the phone's Back key goes to the tab before); the last tab is remembered.
const TABS=['control','program','h4','machine','settings'];let curTab='';
function tab(t,noHist){if(!TABS.includes(t))t='control';if(t==curTab)return;curTab=t;
 document.querySelectorAll('main>section').forEach(s=>s.hidden=s.dataset.tab!=t);
 document.querySelectorAll('#tabs button').forEach(b=>b.classList.toggle('on',b.dataset.t==t));
 if(t=='machine'){logRead=logSeen;tabBadges()}if(t=='program')drawPath();
 try{localStorage.setItem('h4tab',t)}catch(e){}window.scrollTo(0,0);
 if(!noHist)try{history.pushState({t},'','#'+t)}catch(e){}}
addEventListener('popstate',e=>tab((e.state&&e.state.t)||location.hash.slice(1)||'control',true));
let logRead=-1;function tabBadges(){const b=q=>document.querySelector('#tabs button[data-t="'+q+'"]');
 const m=b('machine'),p=b('program');let d=m.querySelector('.dot');const unread=logRead>=0&&st.log!==undefined&&st.log>logRead&&curTab!='machine';
 if(unread&&!d){d=document.createElement('i');d.className='dot';m.appendChild(d)}if(!unread&&d)d.remove();
 let c=p.querySelector('.pct');const run=st.running&&st.progress!==undefined;if(run&&!c){c=document.createElement('i');c.className='pct';p.appendChild(c)}
 if(run)c.textContent=Math.floor(st.progress*100)+'%';else if(c)c.remove()}
// swipe left / right changes the tab (not on the jog wheel, the drawing, fields or the H4 keys)
(function(){let x0=null,y0=0,t0=0;addEventListener('touchstart',e=>{const t=e.target;
 x0=t.closest('.wheel,canvas,input,select,textarea,.keys,.lcd,.chips,.obar,table')?null:e.touches[0].clientX;y0=e.touches[0].clientY;t0=Date.now()},{passive:true});
 addEventListener('touchend',e=>{if(x0===null)return;const dx=e.changedTouches[0].clientX-x0,dy=e.changedTouches[0].clientY-y0;x0=null;
  if(Date.now()-t0<600&&Math.abs(dx)>80&&Math.abs(dx)>2*Math.abs(dy)){const i=TABS.indexOf(curTab)+(dx<0?1:-1);if(i>=0&&i<TABS.length)tab(TABS[i])}},{passive:true})})();
function ctlJogShow(){$('chips').innerHTML=STEPS.map(v=>`<button class="${v==jogStep?'on':''}" onclick="setStep(${v})">${v<1?String(v).slice(1):v}</button>`).join('');
 const u=jogAxis=='C'?'°':(st.inch?' in':' mm');$('stepTxt').textContent=jogStep+u;$('wAxis').textContent=jogAxis;$('wAxis').style.color=AXC[jogAxis];$('wStep').textContent='±'+jogStep.toFixed(3)+u}
function setStep(v){jogStep=v;try{localStorage.setItem('h4step',v)}catch(e){}ctlJogShow()}
function jogDetent(n){jogPending+=n;jogSend()}
// one jog move at a time: the next one goes when a status read after it shows the machine idle again
async function jogSend(){if(jogBusy||!jogPending||st.running||polls<jogPoll+2)return;const d=+(jogPending*jogStep).toFixed(4);jogPending=0;jogBusy=true;
 const f=jogAxis=='C'?gp.n*gp.c:gp.n;
 try{await post('/mdi',new URLSearchParams({line:'G91 G1 '+jogAxis+d+' F'+f}))}catch(e){msg(e.message);jogPending=0}
 jogPoll=polls;jogBusy=false}
setInterval(jogSend,150);
(function(){let t='';for(let i=0;i<40;i++){const a=i*9*Math.PI/180,r1=i%5?78:72,x1=100+r1*Math.sin(a),y1=100-r1*Math.cos(a),x2=100+86*Math.sin(a),y2=100-86*Math.cos(a);
 t+=`<line x1="${x1.toFixed(1)}" y1="${y1.toFixed(1)}" x2="${x2.toFixed(1)}" y2="${y2.toFixed(1)}" stroke="${i%5?'var(--dim)':'var(--fg)'}" opacity="${i%5?0.45:0.7}" stroke-width="2"/>`}
 $('ticks').innerHTML=t;const w=$('wheel');let last=null,acc=0;
 const ang=e=>{const r=w.getBoundingClientRect();return Math.atan2(e.clientX-r.left-r.width/2,-(e.clientY-r.top-r.height/2))*180/Math.PI};
 w.addEventListener('pointerdown',e=>{w.setPointerCapture(e.pointerId);last=ang(e)});
 w.addEventListener('pointermove',e=>{if(last===null)return;const a=ang(e);let d=a-last;if(d>180)d-=360;if(d<-180)d+=360;last=a;acc+=d;wheelAngle+=d;
  $('ticks').setAttribute('transform','rotate('+wheelAngle.toFixed(1)+' 100 100)');while(acc>=9){acc-=9;jogDetent(1)}while(acc<=-9){acc+=9;jogDetent(-1)}});
 for(const ev of['pointerup','pointercancel'])w.addEventListener(ev,()=>{last=null;acc=0});
 $('jp').onclick=()=>jogDetent(1);$('jm').onclick=()=>jogDetent(-1)})();
const MACRO_DEFAULT=[{n:'Unlock ($X)',l:'$X'},{n:'Z up 5 mm',l:'G91 G0 Z5'},{n:'Touch off Z',l:'G10 L20 P0 Z0'},{n:'C to 0°',l:'G90 G0 C0'}];
let macros=MACRO_DEFAULT;try{const m=JSON.parse(localStorage.getItem('h4macros')||'null');if(Array.isArray(m))macros=m}catch(e){}
function macrosShow(){$('macros').innerHTML=macros.map((m,i)=>`<button onclick="runMacro(${i})" title="${m.l.replace(/"/g,'&quot;')}">${m.n.replace(/</g,'&lt;')}</button>`).join('')+
 '<button onclick="editMacros()">Edit…</button>'}
function toggleMacros(){const h=!$('macros').hidden;$('macros').hidden=h;$('bMac').classList.toggle('on',!h);macrosShow()}
function runMacro(i){const m=macros[i];if(confirm('Run '+m.n+': '+m.l+' ?'))gpMdi(m.l)}
function editMacros(){const t=prompt('One macro per line: name = G-code line (empty = defaults)',macros.map(m=>m.n+' = '+m.l).join(' | '));if(t===null)return;
 macros=t.trim()?t.split('|').map(x=>x.split('=')).filter(p=>p.length>1).map(p=>({n:p[0].trim(),l:p.slice(1).join('=').trim()})):MACRO_DEFAULT;
 try{localStorage.setItem('h4macros',JSON.stringify(macros))}catch(e){}macrosShow()}
function ctlShow(){const s=st.state||'-';$('cState').textContent=s.toUpperCase();$('cState').style.borderColor={Run:'#2e9e5b',Hold:'#d9a21b',Stopping:'#d64040'}[s]||'#2f6db3';
 $('cWcs').textContent=st.wcs||'G54';$('cUnits').textContent=st.inch?'inch':'mm';$('cRssi').textContent=st.rssi?st.rssi+' dBm':'';
 $('cHold').textContent=s=='Hold'?'RESUME':'HOLD';if(st.fo!==undefined){$('foTxt').textContent=st.fo+'%';$('foBar').style.width=(st.fo/2)+'%';$('roTxt').textContent=st.ro+'%';$('roBar').style.width=st.ro+'%'}
 axesShow()}
ctlJogShow();if(DEMO)$('title').textContent='H4 CNC · DEMO';
{let t0=location.hash.slice(1);if(!TABS.includes(t0))try{t0=localStorage.getItem('h4tab')}catch(e){}t0=TABS.includes(t0)?t0:'control';
 tab(t0,true);try{history.replaceState({t:t0},'','#'+t0)}catch(e){}}
listFiles();loadCfg();poll();
</script></body></html>
)rawpage";
