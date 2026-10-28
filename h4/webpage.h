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
/* The page never scrolls: every screen fits the window; what does not fit goes to sub-tabs, pages (‹ 1/2 ›)
   or a pop-up. Portrait: tab bar at the bottom; landscape: tab rail on the left. */
*{box-sizing:border-box}html,body{height:100%;margin:0;overflow:hidden;overscroll-behavior:none}
body{background:var(--bg);color:var(--fg);font:15px system-ui,sans-serif;-webkit-tap-highlight-color:transparent}
#app{position:fixed;inset:0;display:grid;grid-template:"h" auto "m" minmax(0,1fr) "t" auto/minmax(0,1fr);
 padding:env(safe-area-inset-top) env(safe-area-inset-right) 0 env(safe-area-inset-left)}
header{grid-area:h;display:flex;gap:8px;align-items:center;padding:6px 10px;border-bottom:1px solid var(--line);min-width:0}
.hinfo{font-size:13px;color:var(--dim);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;min-width:0}.hinfo b{color:var(--fg)}
.demo{font:700 11px system-ui;color:#000;background:var(--warn);border-radius:6px;padding:2px 6px}
.pill{padding:4px 10px;border-radius:14px;border:1px solid #2f6db3;color:#9ecbff;font-weight:700;font-size:13px;letter-spacing:.04em;white-space:nowrap}
.grow{flex:1}.hstop{padding:7px 16px;font-weight:800;letter-spacing:.05em;background:var(--bad);color:#fff;border-color:var(--bad)}
main{grid-area:m;position:relative;min-height:0;overflow:hidden}
.scr{position:absolute;inset:0;padding:8px;display:flex;flex-direction:column;gap:8px;min-height:0}.scr[hidden],[hidden]{display:none!important}
.tabs{grid-area:t;display:grid;grid-template-columns:repeat(5,1fr);background:var(--card);border-top:1px solid var(--line);padding-bottom:env(safe-area-inset-bottom)}
.tabs button{position:relative;border:0;border-radius:0;background:none;padding:6px 2px 5px;font-size:11px;color:var(--dim);display:flex;flex-direction:column;align-items:center;gap:2px}
.tabs svg{width:24px;height:24px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.tabs button.on{color:var(--or)}.tabs button.on::before{content:'';position:absolute;top:0;left:22%;right:22%;height:3px;border-radius:0 0 3px 3px;background:var(--or)}
.tabs .dot{position:absolute;top:4px;left:calc(50% + 8px);width:9px;height:9px;border-radius:5px;background:var(--bad)}.tabs .pct{position:absolute;top:3px;left:calc(50% + 10px);font:700 10px ui-monospace,monospace;font-style:normal;color:var(--ok)}
button,input[type=submit]{font:inherit;padding:8px 10px;border-radius:8px;border:1px solid var(--line);background:var(--btn);color:var(--fg);cursor:pointer;min-width:0}
button:active{filter:brightness(1.4)}button:disabled{opacity:.4}
input[type=text],input[type=password],input[type=number],select{font:inherit;padding:7px;border-radius:6px;border:1px solid var(--line);background:var(--in);color:var(--fg);width:100%;min-width:0}
input[type=color]{width:100%;height:34px;padding:2px;border-radius:6px;border:1px solid var(--line);background:var(--in)}
input[type=file]{min-width:0;max-width:100%;font-size:13px}
.small{font-size:12px;color:var(--dim)}code{font-size:13px;word-break:break-all}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:10px;display:flex;flex-direction:column;gap:8px;min-height:0;min-width:0}
h2{font-size:13px;margin:0;color:var(--dim);font-weight:600;text-transform:uppercase;letter-spacing:.05em;display:flex;align-items:center;gap:8px}
.row{display:flex;gap:6px;flex-wrap:wrap;align-items:center}.row>*{flex:1 1 auto}
.bar{height:10px;background:var(--line);border-radius:5px;overflow:hidden;flex:none}.bar div{height:100%;background:var(--acc);width:0}
.start{background:var(--ok)}.pause{background:var(--warn);color:#000}.stop{background:var(--bad);font-weight:700;font-size:20px;padding:12px}
.info{border-radius:50%;width:26px;height:26px;padding:0;font:700 14px Georgia,serif;flex:none}
/* sub-tabs inside a screen; on a wide landscape screen all panes are shown side by side */
.subtabs{display:flex;gap:4px;flex:none}.subtabs button{flex:1;padding:7px 4px;font-size:14px;background:var(--btn2)}
.subtabs button.on{border-color:var(--or);box-shadow:inset 0 -3px 0 var(--or)}
.panes{flex:1;min-height:0;display:flex}.pane{flex:1;min-width:0;min-height:0;display:flex;flex-direction:column;gap:8px}
/* pages instead of scrolling: .pg holds the items, .pgc the ‹ 1/2 › under it */
.pg{flex:1;min-height:0;overflow:hidden;display:flex;flex-direction:column;gap:6px}
.pgc{display:flex;justify-content:center;align-items:center;gap:14px;flex:none;font-size:13px;color:var(--dim)}.pgc button{padding:4px 18px;font-size:18px}
.fr{display:grid;grid-template-columns:minmax(0,1fr) min(150px,42%);gap:8px;align-items:center;font-size:14px}
.fr.chk{grid-template-columns:minmax(0,1fr) auto}.fr input[type=checkbox]{width:22px;height:22px}
.nope{display:none!important}.changed{outline:2px solid var(--warn)}
.file{display:grid;grid-template-columns:minmax(0,1fr) auto;gap:6px;align-items:center;font-size:14px;border-bottom:1px solid var(--line);padding-bottom:5px}
.file label{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.file .b{display:flex;gap:4px}.file .b button{padding:5px 8px;font-size:13px}
.logl{font:12px ui-monospace,monospace;color:var(--dim);white-space:pre-wrap;word-break:break-word}
canvas{background:#000;border-radius:8px;display:block;margin:auto;max-width:100%;max-height:100%}
.cvbox{flex:1;min-height:0;display:flex;align-items:center;justify-content:center}
.lcd{background:#0b1f10;color:#7cf08f;font:600 16px ui-monospace,monospace;padding:8px;border-radius:8px;white-space:pre;overflow:hidden;line-height:1.3;flex:none}
.keys{flex:1;min-height:0;display:grid;grid-template-columns:repeat(5,1fr);grid-auto-rows:1fr;gap:5px}.keys button{padding:2px;font-size:13px}
.keys button.hold{touch-action:none;user-select:none;-webkit-user-select:none}.keys button.down{background:var(--acc)}
/* control screen */
.ctl{background:linear-gradient(180deg,var(--ctl1),var(--ctl2));display:grid;gap:8px;
 grid-template:"axes" auto "chips" auto "jog" minmax(0,1fr) "act" auto/minmax(0,1fr)}
.axes{grid-area:axes;display:flex;flex-direction:column;gap:6px;min-width:0}
.axis{display:grid;grid-template-columns:28px minmax(0,1fr) auto;align-items:center;gap:2px 8px;padding:5px 8px;border-radius:10px;
 background:var(--tile);border:1px solid var(--tileb);border-left:5px solid var(--c);cursor:pointer}
.axis.sel{border-color:var(--c);box-shadow:0 0 12px -4px var(--c)}
.axis .l{grid-row:1/3;font:800 24px system-ui;color:var(--c)}
.axis .v{font:600 min(var(--dro),9vw,5.2vh) ui-monospace,monospace;text-align:right;white-space:nowrap;overflow:hidden;line-height:1.1}
.axis .v small{font-size:12px;color:var(--dim);margin-left:3px}.axis .m{grid-column:2;font:11px ui-monospace,monospace;color:var(--dim);text-align:right}
.axis .z{grid-row:1/3;grid-column:3;font-size:11px;padding:6px 8px;line-height:1.1}
.chips{grid-area:chips;display:grid;grid-auto-flow:column;grid-auto-columns:1fr;gap:4px}.chips button{padding:7px 0;font:600 13px ui-monospace,monospace}
.chips button.on{background:var(--or);color:#000;border-color:var(--or)}.chips .lk.on{background:var(--ok);border-color:var(--ok);color:#fff}
.jog{grid-area:jog;min-height:0;display:flex;align-items:stretch;justify-content:center}
.pad{flex:1;display:grid;gap:6px;grid-template-columns:repeat(3,minmax(0,150px)) minmax(0,130px);grid-template-rows:repeat(3,minmax(40px,140px));justify-content:center;align-content:center;
 grid-template-areas:"tl zp tr cp" "xm c xp cl" "bl zm br cm";touch-action:none;user-select:none;-webkit-user-select:none}
.pad button{font:800 20px system-ui;color:var(--c);background:var(--btn2);border:1px solid var(--tileb);border-bottom:3px solid var(--c);touch-action:none;padding:0}
.pad button.down{background:var(--c);color:#000}.pad .pc{font:600 13px ui-monospace,monospace;color:var(--dim);border-bottom-color:var(--tileb);display:flex;align-items:center;justify-content:center;text-align:center}
.pad .cl{grid-area:cl;display:flex;align-items:center;justify-content:center;font:800 16px system-ui;color:var(--c)}
.locked .pad button.pb,.locked .wheel{opacity:.3}
.wheelbox{flex:1;display:flex;align-items:center;justify-content:center;gap:10px;min-height:0}
.round{width:52px;height:52px;border-radius:50%;font-size:24px;padding:0;background:var(--btn2);flex:none}
.wheel{position:relative;height:100%;max-height:min(100%,60vw,300px);aspect-ratio:1;touch-action:none;user-select:none;-webkit-user-select:none}
.wheel svg{width:100%;height:100%}.wcenter{position:absolute;inset:26%;border-radius:50%;background:radial-gradient(var(--w1),var(--w2));
 display:flex;flex-direction:column;align-items:center;justify-content:center;border:1px solid var(--wb);pointer-events:none}
.wcenter b{font:800 34px system-ui;color:var(--or)}.wcenter span{font:12px ui-monospace,monospace;color:var(--fg);white-space:nowrap}
.act{grid-area:act;display:grid;grid-template-columns:repeat(4,1fr);gap:6px}.act button{padding:9px 2px;font-size:14px;background:var(--btn2);white-space:nowrap;overflow:hidden}
.act .go{color:#36d399;font-weight:700}.act .hold{color:var(--warn);font-weight:700}.act b{color:var(--or)}
.cprog{grid-area:prog;display:none}
@media (orientation:landscape){
 #app{grid-template:"t h" auto "t m" minmax(0,1fr)/76px minmax(0,1fr)}
 .tabs{grid-template-columns:1fr;grid-auto-rows:1fr;border-top:0;border-right:1px solid var(--line);padding:env(safe-area-inset-top) 0 env(safe-area-inset-bottom)}
 .tabs button.on::before{top:22%;bottom:22%;left:0;right:auto;width:3px;height:auto;border-radius:0 3px 3px 0}
 .ctl{grid-template:"axes jog" auto "chips jog" auto "act jog" auto ". jog" minmax(0,1fr)/minmax(0,1fr) minmax(0,1fr)}
 .axis .v{font-size:min(var(--dro),4.5vw,7.5vh)}
}
@media (orientation:landscape) and (min-width:1000px){
 .ctl{grid-template:"axes jog prog" auto "chips jog prog" auto "act jog prog" auto ". jog prog" minmax(0,1fr)/minmax(0,1.1fr) minmax(0,1fr) minmax(0,.9fr)}
 .cprog{display:flex}
 .subtabs{display:none}.pane[hidden]{display:flex!important}.panes{gap:8px}
}
@media (orientation:landscape) and (max-height:460px){
 header{padding:3px 10px}.hstop{padding:5px 14px}.scr{padding:6px;gap:6px}.ctl{gap:5px}
 .axis{padding:2px 8px}.axis .l{font-size:20px}.axis .v{font-size:min(var(--dro),4.5vw,7vh)}.axis .z{padding:4px 6px}
 .chips button{padding:5px 0}.act{gap:4px}.act button{padding:6px 2px;font-size:13px}
 .pane[data-p=run]{flex-flow:column wrap;align-content:flex-start;column-gap:10px}.pane[data-p=run]>*{width:calc(50% - 5px)}
 .stop{font-size:16px;padding:8px}
 .pane[data-p=keys]{display:grid;grid-template:"h h" auto "l k" minmax(0,1fr)/auto minmax(0,1fr)}.pane[data-p=keys] h2{grid-area:h}
 .pane[data-p=keys] .lcd{grid-area:l;align-self:start;font-size:13px}.pane[data-p=keys] .keys{grid-area:k;gap:3px}.keys button{font-size:12px}
 .slist button{padding:8px 12px}
}
/* settings: a list of pages; on a wide screen the list and the open page side by side */
.sets{display:flex;gap:8px;flex:1;min-height:0}.slist{flex:1;display:flex;flex-direction:column;gap:6px;min-height:0}
.slist .pg{gap:6px}.slist button{text-align:left;padding:11px 12px;background:var(--btn2);display:flex;justify-content:space-between}.slist button.on{border-color:var(--or)}
.spanel{flex:1.6;min-width:0;min-height:0;display:flex;flex-direction:column;gap:8px}.spage{flex:1;min-height:0;display:flex;flex-direction:column;gap:8px}
.sets.open .slist{display:none}.sets:not(.open) .spanel{display:none}.sback{display:inline-block}
@media (orientation:landscape) and (min-width:700px){.sets .slist,.sets .spanel{display:flex!important}.sback{display:none}}
/* pop-up */
.sheet{position:fixed;inset:0;z-index:8;background:#0009;display:flex;align-items:flex-start;justify-content:center;padding:10vh 12px 12px}
.sbox{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px;width:100%;max-width:440px;max-height:80vh;display:flex;flex-direction:column;gap:10px;overflow:hidden}
.shead{display:flex;justify-content:space-between;align-items:center;font-weight:700}.sgrid{display:grid;grid-template-columns:repeat(3,1fr);gap:6px}.sgrid button{padding:12px 4px}
.toast{position:fixed;left:12px;right:12px;bottom:calc(68px + env(safe-area-inset-bottom));z-index:9;background:#222;color:#fff;border:1px solid var(--line);border-radius:10px;padding:10px 12px;font-size:14px;box-shadow:0 6px 20px #0008;transition:opacity .3s;max-width:560px;margin:auto}
@media (orientation:landscape){.toast{bottom:12px;left:88px}}
.toast.off{opacity:0;pointer-events:none}
</style></head><body>
<div id="app">
<header><span class="pill" id="cState">-</span><span class="hinfo"><b id="cWcs">G54</b> · <span id="cUnits">mm</span> <span id="cRssi"></span></span>
<span class="demo" id="demoTag" hidden>DEMO</span><span class="grow"></span><button class="hstop" onclick="cmd('stop')">STOP</button></header>
<main>
<section class="scr ctl" data-tab="control" id="ctl">
<div class="axes" id="axes"></div>
<div class="chips" id="chips"></div>
<div class="jog"><div class="pad" id="pad">
<button class="pb" style="grid-area:zp" data-a="Z" data-d="1">Z+</button><button class="pb" style="grid-area:cp" data-a="C" data-d="1">C+</button>
<button class="pb" style="grid-area:xm" data-a="X" data-d="-1">X&minus;</button><div class="pc" style="grid-area:c" id="padC"></div>
<button class="pb" style="grid-area:xp" data-a="X" data-d="1">X+</button><span class="cl" id="padCl">C</span>
<button class="pb" style="grid-area:zm" data-a="Z" data-d="-1">Z&minus;</button><button class="pb" style="grid-area:cm" data-a="C" data-d="-1">C&minus;</button></div>
<div class="wheelbox" id="wheelbox" hidden><button class="round" id="jm">&minus;</button><div class="wheel" id="wheel"><svg viewBox="0 0 200 200"><circle cx="100" cy="100" r="92" fill="var(--w2)" stroke="var(--wb)"/><g id="ticks"></g>
<path d="M100 14 A86 86 0 0 1 172 55" fill="none" stroke="var(--or)" stroke-width="4" stroke-linecap="round"/></svg>
<div class="wcenter"><b id="wAxis">X</b><span id="wStep"></span></div></div><button class="round" id="jp">+</button></div></div>
<div class="act"><button class="go" onclick="startSel()">START</button><button class="hold" id="cHold" onclick="cmd(st.state=='Hold'?'resume':'pause')">HOLD</button>
<button onclick="ovrSheet()">F <b id="foTxt">100%</b></button><button onclick="ovrSheet()">R <b id="roTxt">100%</b></button>
<button onclick="homeAll()">Home</button><button onclick="macroSheet()">Macros</button><button onclick="mdiSheet()">MDI</button><button onclick="tab('program');sub('program','files')">Files</button></div>
<div class="card cprog"><h2>Program</h2><div><b id="cpName">no program</b></div><div class="bar"><div id="cpBar"></div></div>
<div class="small"><span id="cpPct">0 %</span> &middot; <code id="cpLine"></code></div><h2>Messages</h2><div class="pg" id="cpLog"></div></div>
</section>

<section class="scr" data-tab="program" hidden>
<div class="subtabs" data-s="program"><button data-p="run">Run</button><button data-p="files">Files</button><button data-p="path">Path</button></div>
<div class="panes">
<div class="pane card" data-p="run"><h2>Program</h2>
<div><b id="pname">no program</b> <span class="small" id="stext"></span></div><div class="bar"><div id="pbar"></div></div>
<div class="small"><span id="ppct">0 %</span> &middot; <code id="pline"></code></div>
<div class="row"><button class="start" id="bStart" onclick="startSel()">Start</button><button class="pause" onclick="cmd('pause')">Pause</button><button onclick="cmd('resume')">Resume</button></div>
<button class="stop" onclick="cmd('stop')">STOP</button>
<div class="row"><span>Feed <b id="fo">100</b> %</span><button onclick="ovr(146)">&minus;10</button><button onclick="ovr(144)">100</button><button onclick="ovr(145)">+10</button></div>
<div class="row"><span>Rapids <b id="ro">100</b> %</span><button onclick="ovr(151)">25</button><button onclick="ovr(150)">50</button><button onclick="ovr(149)">100</button></div>
<form class="row" id="mdiform"><input type="text" id="mdi" placeholder="G-code line, e.g. G0 C90" autocomplete="off" style="flex:3"><input type="submit" id="bMdi" value="Run"></form>
<div id="msg" class="small" style="color:var(--warn)"></div></div>
<div class="pane card" data-p="files" hidden><h2>Files <span class="grow"></span><button class="info" onclick="infoSheet('files')">i</button></h2>
<form id="up" class="row"><input type="file" id="file" accept=".nc,.ngc,.gcode,.tap,.txt"><input type="submit" value="Upload"></form>
<div class="bar"><div id="ubar"></div></div><div class="pg" id="files"></div><div class="pgc"></div><div class="small" id="fs"></div></div>
<div class="pane card" data-p="path" hidden><h2>Path</h2><div class="cvbox" id="cvbox"><canvas id="cv" width="600" height="600"></canvas></div>
<div class="small" id="pathinfo">Select a file and press Draw.</div></div>
</div></section>

<section class="scr" data-tab="h4" hidden>
<div class="subtabs" data-s="h4"><button data-p="keys">H4 keys</button><button data-p="pad">Gamepad</button></div>
<div class="panes">
<div class="pane card" data-p="keys"><h2>H4 display and keys <span class="grow"></span><button class="info" onclick="infoSheet('keys')">i</button></h2>
<div class="lcd" id="lcd"></div><div class="keys" id="keys"></div></div>
<div class="pane card" data-p="pad" hidden><h2>Gamepad <span class="grow"></span><button class="info" onclick="infoSheet('pad')">i</button></h2><div id="gpstate" class="small">No gamepad.</div>
<div class="pg"><div class="fr"><label for="gpN">Jog speed (mm/min)</label><input type="number" id="gpN" step="any"></div>
<div class="fr"><label for="gpF">Speed with R1 (mm/min)</label><input type="number" id="gpF" step="any"></div>
<div class="fr"><label for="gpC">C speed = speed x</label><input type="number" id="gpC" step="any"></div>
<div class="fr chk"><label for="gpIX">Reverse X</label><input type="checkbox" id="gpIX"></div><div class="fr chk"><label for="gpIZ">Reverse Z</label><input type="checkbox" id="gpIZ"></div>
<div class="fr chk"><label for="gpIC">Reverse C</label><input type="checkbox" id="gpIC"></div></div><div class="pgc"></div>
<div class="row" id="gpapp" hidden><button onclick="H4App.changeAddress()">H4 address</button></div></div>
</div></section>

<section class="scr" data-tab="machine" hidden>
<div class="subtabs" data-s="machine"><button data-p="pos">Position</button><button data-p="diag">Diagnostics</button><button data-p="log">Messages</button></div>
<div class="panes">
<div class="pane card" data-p="pos"><h2>Position</h2>
<div class="pg"><div style="font:600 22px ui-monospace,monospace">X <span id="x">-</span></div><div style="font:600 22px ui-monospace,monospace">Z <span id="z">-</span></div>
<div style="font:600 22px ui-monospace,monospace"><span id="cl">C</span> <span id="c">-</span></div>
<div class="small" id="mach"></div><div class="small" id="info"></div><div class="small">Inputs (terminal A2): <span id="ins"></span></div></div><div class="pgc"></div>
<button id="bHome" onclick="homeAll()">Home All</button></div>
<div class="pane card" data-p="diag" hidden><h2>Diagnostics <span class="grow"></span><button class="info" onclick="infoSheet('diag')">i</button></h2>
<div class="pg"><div id="diag" class="small">-</div></div><button onclick="cmd('diagReset')">Clear</button></div>
<div class="pane card" data-p="log" hidden><h2>Messages</h2><div class="pg" id="log"></div><div class="pgc"></div></div>
</div></section>

<section class="scr" data-tab="settings" hidden><div class="sets" id="sets">
<div class="slist"><div class="pg" id="slist"></div><div class="pgc"></div></div>
<div class="spanel card"><h2><button class="sback" onclick="history.back()">&lsaquo; Back</button><span id="sTitle"></span></h2>
<div class="spage" data-sp="look"><div class="pg">
<div class="fr"><label for="lkTheme">Theme</label><select id="lkTheme"><option value="dark">Dark</option><option value="light">Light</option></select></div>
<div class="fr"><label for="lkAcc">Accent colour</label><input type="color" id="lkAcc"></div>
<div class="fr"><label for="lkX">X colour</label><input type="color" id="lkX"></div><div class="fr"><label for="lkZ">Z colour</label><input type="color" id="lkZ"></div>
<div class="fr"><label for="lkC">C colour</label><input type="color" id="lkC"></div>
<div class="fr"><label for="lkSize">Number size</label><select id="lkSize"><option value="34">Normal</option><option value="44">Large</option><option value="56">Very large</option></select></div>
<button onclick="lookReset()">Back to the original colours</button><div class="small">Kept in this phone or browser only.</div></div><div class="pgc"></div></div>
<div class="spage" data-sp="screen"><div class="pg">
<div class="fr"><label for="scJog">Jog with</label><select id="scJog"><option value="pad">Buttons</option><option value="wheel">Wheel</option></select></div>
<div class="fr"><label for="scLock">Jog lock</label><select id="scLock"><option value="1">On</option><option value="0">Off</option></select></div>
<div class="fr nope" id="scOriRow"><label for="scOri">Screen orientation</label><select id="scOri"><option value="auto">Automatic</option><option value="landscape">Landscape</option><option value="portrait">Portrait</option></select></div>
<div class="small">Jog lock: the jog buttons and the wheel work only after the lock button &#x1F512; is pressed, and lock again 15 s after the last move. With the buttons, a tap moves one step; with <b>Cont</b> the axis moves while the button is held, at the jog speed of the gamepad settings, and stops when it is released.</div></div><div class="pgc"></div></div>
<div id="cfg"></div>
<div class="spage" data-sp="wifi"><div class="pg">
<input type="text" id="ssid" placeholder="WiFi network name"><input type="password" id="pw" placeholder="password (empty = keep)">
<button onclick="saveWifi()">Save WiFi and restart</button><div class="small" id="wifi"></div></div></div>
<div class="spage" data-sp="app"><div class="pg">
<div>H4 CNC app <b id="appVer">-</b>. It looks for a newer version by itself each time it opens.</div>
<button onclick="H4App.checkAppUpdate()">Check for an app update</button>
<div class="small">A newer version is offered for download; Android installs it over this one and keeps the settings.</div></div></div>
<div class="spage" data-sp="fw"><div class="pg">
<div>Firmware <b id="fwVer">-</b>. The settings of the machine stay through an update.</div>
<div class="row nope" id="fwApp"><button onclick="H4App.checkFirmware()">Check for a firmware update (GitHub)</button></div>
<input type="file" id="fw" accept=".bin"><button onclick="uploadFw()">Update firmware (.bin)</button><div class="small" id="fwmsg"></div></div></div>
</div></div></section>
</main>
<nav class="tabs" id="tabs"></nav>
</div>
<div class="sheet" id="sheet" hidden onclick="if(event.target==this)sheetClose()"><div class="sbox"><div class="shead"><span id="shTitle"></span><button onclick="sheetClose()">&#x2715;</button></div><div id="shBody" style="display:flex;flex-direction:column;gap:8px;min-height:0"></div></div></div>
<div class="toast off" id="toast"></div>
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
const isApp=!!window.H4App;
let toastT=0;function msg(t){$('msg').textContent=t||'';if(!t)return;const e=$('toast');e.textContent=t;e.classList.remove('off');clearTimeout(toastT);toastT=setTimeout(()=>e.classList.add('off'),4000)}
async function post(url,body){const r=await fetch(url,{method:'POST',body});const t=await r.text();if(!r.ok)throw new Error(t);return t}
async function cmd(c,extra){try{msg(await post('/cmd?do='+c+(extra||'')))}catch(e){msg(e.message)}}
function startSel(){if(!selected){msg('Select a file first (Program, Files).');return}
 if(confirm('Start '+selected+' on the machine?'))cmd('start','&name='+encodeURIComponent(selected))}
let polls=0;async function poll(){try{const r=await fetch('/status');st=await r.json();polls++;show()}catch(e){$('cState').textContent='NO CONNECTION'}
 setTimeout(poll,350)}
function ovr(c){cmd('ovr','&code='+c)}
let benchSeen=-1;function bench(n){cmd('bench','&name='+encodeURIComponent(n))}
function diagShow(d){if(!d)return;const per=d.rate?1e6/d.rate:0;
 const ok=d.late==0&&(!d.rate||d.isrUs<0.7*per)&&!st.underruns;
 $('diag').innerHTML=(ok?'<b>OK</b>':'<b style="color:#d64040">CHECK</b>')+'<br>Step interrupt, longest: '+d.isrUs+' µs'+(d.rate?' (time between steps at the highest rate: '+per.toFixed(1)+' µs)':'')+
  '<br>Highest step rate: '+d.rate+' /s (limit '+d.limit+' /s)<br>Started late (over 10 µs): '+d.late+' times, latest '+d.lateUs+' µs<br>Step buffer ran empty: '+st.underruns+'<br>Steps given: '+d.steps}
function show(){const u=st.inch?' in':' mm';diagShow(st.diag);ctlShow();if(st.fw)$('fwVer').textContent=st.fw;
 if(st.benchN!==undefined){if(benchSeen>=0&&st.benchN!=benchSeen)msg(st.bench);benchSeen=st.benchN}if(st.fo!==undefined){$('fo').textContent=st.fo;$('ro').textContent=st.ro}
 $('x').textContent=st.x.toFixed(3)+u;$('z').textContent=st.z.toFixed(3)+u;$('c').textContent=st.cActive?st.c.toFixed(3)+'°':'-';
 $('info').textContent='Feed '+st.feed+' /min · spindle '+st.rpm+' rpm · step buffer empties: '+st.underruns+(st.estop?' · EMERGENCY STOP':'')+(st.gcodeMode?'':' · the H4 is not in GCODE mode');
 $('wifi').textContent=st.wifi;$('pname').textContent=st.program||'no program';$('stext').textContent=st.text;$('cpName').textContent=st.program||'no program';
 const p=st.running||st.programLoaded?st.progress:0;$('pbar').style.width=$('cpBar').style.width=(p*100)+'%';$('ppct').textContent=$('cpPct').textContent=(p*100).toFixed(1)+' %';$('pline').textContent=$('cpLine').textContent=st.line;
 $('bStart').disabled=!!st.running;$('fs').textContent=st.fsTotal?('Free '+((st.fsTotal-st.fsUsed)/1048576).toFixed(2)+' MB of '+(st.fsTotal/1048576).toFixed(2)+' MB · program memory '+(st.psram?(st.psram/1048576).toFixed(2)+' MB (PSRAM)':'without PSRAM: up to '+(st.heap/1024).toFixed(0)+' kB')):'';
 $('lcd').textContent=(st.lcd||[]).join('\n');
 const H=st.homed||[0,0,0];$('mach').textContent='Homed: X '+(H[0]?'yes':'no')+' · Z '+(H[1]?'yes':'no')+' · C '+(H[2]?'yes':'no')+
  (st.mx!==undefined?' · machine X '+st.mx.toFixed(3):'')+(st.mz!==undefined?' · machine Z '+st.mz.toFixed(3):'')+(st.homing?' · HOMING':'');
 $('bHome').disabled=!!st.running||!!st.homing;
 if(st.a2){const N=['A21 home X','A22 home Z','A23 alarm'],A=st.act||[0,0,0],U=st.use||[0,0,0];
  $('ins').textContent=N.map((n,i)=>n+': '+((st.in>>i)&1?'high':'LOW')+(U[i]?(A[i]?' (ACTIVE)':' (ok)'):'')).join(' · ')}
 else $('ins').textContent='used by handwheel 2';$('bMdi').disabled=!!st.running;
 if(st.log!==logSeen){logSeen=st.log;loadLog();if(logRead<0||curTab=='machine')logRead=st.log}tabBadges();
 if(curTab=='program')drawPath()}
async function listFiles(){try{const r=await fetch('/files');if(!r.ok){msg(await r.text());return}files=await r.json();
 $('files').innerHTML=files.length?files.map(f=>`<div class="file"><label><input type="radio" name="sel" value="${f.name}" ${f.name==selected?'checked':''} onchange="selected=this.value"> ${f.name} <span class="small">${(f.size/1024).toFixed(1)} kB</span></label><span class="b"><button onclick="draw('${f.name}')">Draw</button><button onclick="bench('${f.name}')">Test</button><button onclick="del('${f.name}')">&#x2715;</button></span></div>`).join(''):'<div class="small">No files yet: upload one.</div>';
 pagers()}catch(e){msg(e.message)}}
async function del(n){if(!confirm('Delete '+n+'?'))return;try{msg(await post('/delete?name='+encodeURIComponent(n)));if(selected==n)selected='';listFiles()}catch(e){msg(e.message)}}
$('up').onsubmit=e=>{e.preventDefault();const f=$('file').files[0];if(!f)return;const fd=new FormData();fd.append('file',f,f.name);
 const x=new XMLHttpRequest();x.open('POST','/upload');x.upload.onprogress=ev=>{$('ubar').style.width=(ev.loaded/ev.total*100)+'%'};
 x.onload=()=>{msg(x.responseText);$('ubar').style.width='0';selected=f.name;listFiles();f.text().then(t=>{drawn=parse(t,f.name)})};x.onerror=()=>msg('Upload failed');x.send(fd)};
async function draw(n){selected=n;listFiles();try{const r=await fetch('/file?name='+encodeURIComponent(n));if(!r.ok){msg(await r.text());return}drawn=parse(await r.text(),n);sub('program','path')}catch(e){msg(e.message)}}
// Path: polar programs (with C) are drawn seen from the cutter (X radius, C angle); others as Z-X.
function parse(text,name){let abs=true,x=0,z=0,c=0,pts=[],hasC=false,chars=0;
 for(const raw of text.split('\n')){chars+=raw.length+1;const l=raw.replace(/\(.*?\)/g,'').replace(/;.*/,'').toUpperCase();
  if(/G91(?![\d.])/.test(l))abs=false;if(/G90(?![\d.])/.test(l))abs=true;
  const g=(k)=>{const m=l.match(new RegExp(k+'\\s*([-+]?[\\d.]+)'));return m?parseFloat(m[1]):null};
  const X=g('X'),Z=g('Z'),C=g('C');if(X===null&&Z===null&&C===null)continue;if(C!==null)hasC=true;
  if(X!==null)x=abs?X:x+X;if(Z!==null)z=abs?Z:z+Z;if(C!==null)c=abs?C:c+C;pts.push([x,z,c,chars,/G0(?![\d.])/.test(l)])}
 return{name,pts,hasC,total:chars}}
function xy(p,hasC){return hasC?[p[0]*Math.cos(p[2]*Math.PI/180),p[0]*Math.sin(p[2]*Math.PI/180)]:[p[1],p[0]]}
function drawPath(){const cv=$('cv'),box=$('cvbox');if(box.clientWidth){const s=Math.max(50,Math.min(box.clientWidth,box.clientHeight));cv.style.width=cv.style.height=s+'px'}
 const g=cv.getContext('2d');g.clearRect(0,0,cv.width,cv.height);if(!drawn||!drawn.pts.length)return;
 const P=drawn.pts.map(p=>xy(p,drawn.hasC));let x0=1e9,x1=-1e9,y0=1e9,y1=-1e9;for(const[a,b]of P){x0=Math.min(x0,a);x1=Math.max(x1,a);y0=Math.min(y0,b);y1=Math.max(y1,b)}
 if(drawn.hasC){const r=Math.max(Math.abs(x0),Math.abs(x1),Math.abs(y0),Math.abs(y1));x0=y0=-r;x1=y1=r}
 const s=0.9*cv.width/Math.max(x1-x0,y1-y0,1e-3),ox=cv.width/2-s*(x0+x1)/2,oy=cv.height/2+s*(y0+y1)/2,T=([a,b])=>[ox+s*a,oy-s*b];
 const done=(st.program==drawn.name&&(st.running||st.programLoaded))?st.progress*drawn.total:0;
 g.lineWidth=1.2;for(let i=1;i<P.length;i++){const[a,b]=T(P[i-1]),[c2,d]=T(P[i]);g.strokeStyle=drawn.pts[i][3]<=done?'#3a8ee6':(drawn.pts[i][4]?'#553':'#888');g.beginPath();g.moveTo(a,b);g.lineTo(c2,d);g.stroke()}
 if(drawn.hasC){g.strokeStyle='#444';g.beginPath();g.arc(ox,oy,3,0,7);g.stroke()}
 const cur=T(xy([st.x,st.z,st.c],drawn.hasC));g.fillStyle='#e33';g.beginPath();g.arc(cur[0],cur[1],6,0,7);g.fill();
 $('pathinfo').textContent=drawn.name+': '+drawn.pts.length+' moves'+(drawn.hasC?', seen from the cutter (X radius, C angle)':', Z across, X up')}
let logSeen=-1,logLines=[];
async function mdiRun(l){l=l.trim();if(!l)return;try{msg(await post('/mdi',new URLSearchParams({line:l})))}catch(err){msg(err.message)}}
$('mdiform').onsubmit=e=>{e.preventDefault();mdiRun($('mdi').value)};
async function loadLog(){try{const r=await fetch('/log');logLines=await r.json();logShow()}catch(e){}}
function logShow(){const h=a=>a.length?a.map(t=>`<div class="logl">${t.replace(/</g,'&lt;')}</div>`).join(''):'<div class="small">No messages.</div>';
 $('log').dataset.p='last';$('log').innerHTML=h(logLines);$('cpLog').innerHTML=h(logLines);pagers()}
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
// Machine settings: stored in the firmware's units (steps, deci-microns), shown in mm, mm/s, mm/s2. One settings
// page per group; all groups stay in the page so Save sends every value.
let cfg=null;
function cfgVal(k){const it=cfg.items.find(i=>i.k==k);const el=$('c_'+k);return el?(it.t=='b'?(el.checked?1:0):el.value):it.v}
function spu(p){const steps=+cfgVal(p+'Steps');const it=cfg.items.find(i=>i.k==p+'Screw');const el=$('c_'+p+'Screw');
 const screw=el?+el.value*10000:it.v;return steps/(screw/10000)}
function unitText(it){const rot=it.k[0]=='a'&&cfg.items.find(i=>i.k=='aRot').v;const L=rot?'°':'mm';
 if(it.u=='du')return L;if(it.u.startsWith('v:'))return L+'/s';if(it.u.startsWith('a:'))return L+'/s²';return it.u}
function toShow(it,v){if(it.u=='du')return +(v/10000).toPrecision(8);if(it.u.startsWith('v:')||it.u.startsWith('a:'))return +(v/spu(it.u[2])).toPrecision(7);return v}
let cfgGroups=[];
function renderCfg(){const groups={};for(const it of cfg.items)(groups[it.g]=groups[it.g]||[]).push(it);let h='';cfgGroups=Object.keys(groups);
 cfgGroups.forEach((g,gi)=>{h+=`<div class="spage" data-sp="m${gi}" hidden><div class="pg">`;for(const it of groups[g]){const id='c_'+it.k,u=unitText(it);let inp;
  if(it.t=='b')inp=`<input type="checkbox" id="${id}" ${it.v?'checked':''}>`;
  else if(it.t=='a')inp=`<select id="${id}"><option ${it.v=='Z'?'selected':''}>Z</option><option ${it.v=='X'?'selected':''}>X</option></select>`;
  else inp=`<input type="number" step="any" id="${id}" value="${toShow(it,it.v)}">`;
  h+=`<div class="fr${it.t=='b'?' chk':''}"><label for="${id}">${it.l}${u?' ('+u+')':''}</label>${inp}</div>`}
  h+='</div><div class="pgc"></div><div class="row"><button onclick="saveCfg()">Save and restart</button><button onclick="resetCfg()">Defaults</button></div></div>'});
 $('cfg').innerHTML=h;$('cfg').style.display='contents';
 $('cfg').querySelectorAll('input,select').forEach(el=>el.addEventListener('change',()=>el.classList.add('changed')));setList()}
async function loadCfg(){try{const r=await fetch('/config');cfg=await r.json();renderCfg()}catch(e){$('cfg').textContent='Could not load the settings.'}}
async function saveCfg(){if(!cfg)return;const q=new URLSearchParams();
 for(const it of cfg.items){const el=$('c_'+it.k);let v;
  if(it.t=='b')v=el.checked?1:0;else if(it.t=='a')v=el.value;
  else{const x=parseFloat(el.value);if(!isFinite(x)){msg(it.g+', '+it.l+': not a number');return}
   if(it.u=='du')v=Math.round(x*10000);else if(it.u.startsWith('v:')||it.u.startsWith('a:'))v=Math.round(x*spu(it.u[2]));
   else if(it.t=='f')v=x;else v=Math.round(x)}
  q.append(it.k,v)}
 if(!confirm('Save the machine settings (all pages)? Only while the machine is off. The controller restarts.'))return;try{msg(await post('/config',q))}catch(e){msg(e.message)}}
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
const gpApp=isApp;function gpToApp(){if(gpApp)try{H4App.settings(JSON.stringify(gp))}catch(e){}}
if(gpApp){$('fwApp').classList.remove('nope');$('gpapp').hidden=false;window.h4AppGamepad=t=>{$('gpstate').textContent=t};gpToApp()}
let gpPrev=[],gpLast=0,gpBusy=false,gpMoving=false,gpStops=0,gpErr='',gpPad=null;
function gpDz(v,d){const a=Math.abs(v);return a<=d?0:Math.sign(v)*(a-d)/(1-d)}
function gpRumble(){try{gpPad&&gpPad.vibrationActuator&&gpPad.vibrationActuator.playEffect('dual-rumble',{duration:200,strongMagnitude:.5,weakMagnitude:.5})}catch(e){}}
async function gpSend(x,z,c){gpLast=performance.now();gpBusy=true;
 try{const r=await fetch('/jog?x='+x.toFixed(1)+'&z='+z.toFixed(1)+'&c='+c.toFixed(1),{method:'POST'});
  if(!r.ok){const t=await r.text();if(t!=gpErr){gpErr=t;msg('Jog: '+t);gpRumble()}}else gpErr=''}catch(e){}
 gpBusy=false}
function gpMdi(l){post('/mdi',new URLSearchParams({line:l})).then(msg,e=>{msg(e.message);gpRumble()})}
function gpLoop(){let pads=[];
 try{pads=navigator.getGamepads?[...navigator.getGamepads()].filter(p=>p&&p.connected):[]}catch(e){}
 const g=pads[0];gpPad=g||null;const now=performance.now();
 if(!g||document.hidden){if(gpMoving||gpStops>0){if(now-gpLast>=100){gpSend(0,0,0);gpStops=gpMoving?3:gpStops-1;gpMoving=false}}
  $('gpstate').textContent=!window.isSecureContext&&!g?'No gamepad. Connect it, press a button (Chrome: see i).':'No gamepad: connect it, then press a button.';return}
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
 else if(gpStops>0&&now-gpLast>=100&&!padHeld){gpSend(0,0,0);gpStops=gpMoving?2:gpStops-1;gpMoving=false} // stop, sent 3 times
 $('gpstate').textContent=(g.mapping=='standard'?'':'(unknown button layout) ')+g.id+(want?' · moving':l1?' · L1 held':' · hold L1 to move')+(gpErr?' · '+gpErr:'')}
if(!gpApp)setInterval(gpLoop,20);
document.addEventListener('visibilitychange',()=>{if(document.hidden&&(gpMoving||gpStops))gpSend(0,0,0)});
fetch('/wifi').then(r=>r.json()).then(w=>{$('ssid').value=w.ssid}).catch(()=>{});
// Pop-up: overrides, macros, MDI and the longer explanations (instead of making the screens longer).
function sheet(title,html){$('shTitle').textContent=title;$('shBody').innerHTML=html;$('sheet').hidden=false}
function sheetClose(){$('sheet').hidden=true;$('shBody').innerHTML=''}
function ovrSheet(){sheet('Feed and rapids',`<div>Feed <b id="sFo">${st.fo||100}</b> %</div><div class="sgrid"><button onclick="ovr(146)">&minus;10</button><button onclick="ovr(144)">100 %</button><button onclick="ovr(145)">+10</button></div>
 <div>Rapids <b id="sRo">${st.ro||100}</b> %</div><div class="sgrid"><button onclick="ovr(151)">25 %</button><button onclick="ovr(150)">50 %</button><button onclick="ovr(149)">100 %</button></div>`)}
function mdiSheet(){sheet('MDI: one G-code line',`<form class="row" onsubmit="event.preventDefault();mdiRun(this.l.value)"><input type="text" name="l" placeholder="e.g. G0 C90" autocomplete="off" style="flex:3"><input type="submit" value="Run"></form>
 <div class="small">The H4 must be idle. Keep a real emergency stop within reach.</div>`);setTimeout(()=>$('shBody').querySelector('input').focus(),50)}
const INFO={files:'Upload a G-code file (.nc .ngc .gcode .tap .txt), choose it with the round button, then START. Draw shows the path, Test reads and plans the whole file on the controller without moving (it tells whether the controller keeps up with the program).',
 keys:'The same as the keys on the controller. Arrows, GEARS and TURN move while held; if the page loses the connection the key is released within half a second.',
 pad:'PS4 / PS5 gamepad connected to this phone or computer by Bluetooth (or USB). Hold L1 to move: left stick Z, right stick X, L2 / R2 C+ / C-, R1 fast. L1 + Share = Home All, L1 + D-pad down = touch off Z, Options = STOP, PS = unlock ($X). D-pad (without L1): up / down feed +10 / -10 %, left 100 %. Keep the page open in front; it stops when the page is hidden or the WiFi drops. '+(isApp?'':'Best with the H4 CNC Android app. In Chrome on Android instead, once: chrome://flags, Insecure origins treated as secure, add '+location.origin+', Enabled, Relaunch.'),
 diag:'Measured by the controller itself since the program started (or Clear): the step interrupt must stay well below the time between two steps at the highest step rate, it should never start late, and the step buffer should never run empty. Run a program in the air at full speed with WiFi in use to see the real limits.'};
function infoSheet(k){sheet('About',`<div style="line-height:1.4">${INFO[k].replace(/</g,'&lt;')}</div>`)}
// Control screen: DRO with work and machine position, ZERO per axis, jog buttons or wheel, overrides, macros.
let unlocked=false,lockT=0;
const STEPS=[0.001,0.01,0.1,1,10];let jogAxis='X',jogStep=0.1,jogPending=0,jogBusy=false,jogPoll=-9,wheelAngle=0;
try{const v=localStorage.getItem('h4step');jogStep=v=='cont'?'cont':(+v||0.1)}catch(e){}
const SC0={jog:'pad',lock:'1'};let sc=Object.assign({},SC0);try{Object.assign(sc,JSON.parse(localStorage.getItem('h4screen')||'{}'))}catch(e){}
const LOOK0={theme:'dark',acc:'#ff9a1f',X:'#ff9a1f',Z:'#7ad151',C:'#e24bd0',size:'34'};let look=Object.assign({},LOOK0);
try{Object.assign(look,JSON.parse(localStorage.getItem('h4look')||'{}'))}catch(e){}
const AXC={X:look.X,Z:look.Z,C:look.C};
// Appearance: theme, accent and axis colours, number size (stored in the browser)
function lookApply(){document.body.classList.toggle('light',look.theme=='light');const r=document.documentElement.style;
 r.setProperty('--or',look.acc);r.setProperty('--dro',look.size+'px');AXC.X=look.X;AXC.Z=look.Z;AXC.C=look.C;
 document.querySelectorAll('#pad [data-a]').forEach(b=>b.style.setProperty('--c',AXC[b.dataset.a]));$('padCl').style.setProperty('--c',AXC.C);
 const a=$('axes');if(a)a.dataset.n='';if(typeof ctlJogShow=='function'&&$('chips'))ctlJogShow()}
function lookSave(){try{localStorage.setItem('h4look',JSON.stringify(look))}catch(e){}lookApply()}
function lookReset(){look=Object.assign({},LOOK0);lookForm();lookSave()}
function lookForm(){$('lkTheme').value=look.theme;$('lkAcc').value=look.acc;$('lkX').value=look.X;$('lkZ').value=look.Z;$('lkC').value=look.C;$('lkSize').value=look.size}
lookForm();for(const [id,k] of [['lkTheme','theme'],['lkAcc','acc'],['lkX','X'],['lkZ','Z'],['lkC','C'],['lkSize','size']])
 $(id).addEventListener('input',()=>{look[k]=$(id).value;lookSave()});
lookApply();
// Screen and jog settings; the orientation is kept by the app (Android), so it is offered only there.
function scSave(){try{localStorage.setItem('h4screen',JSON.stringify(sc))}catch(e){}scApply()}
function scApply(){const wheel=sc.jog=='wheel';$('pad').hidden=wheel;$('wheelbox').hidden=!wheel;if(wheel&&jogStep=='cont')jogStep=0.1;
 if(sc.lock!='1')unlocked=true;else if(!lockT)unlocked=false;ctlJogShow()}
$('scJog').value=sc.jog;$('scLock').value=sc.lock;$('scJog').onchange=()=>{sc.jog=$('scJog').value;scSave()};$('scLock').onchange=()=>{sc.lock=$('scLock').value;scSave()};
if(isApp)try{$('appVer').textContent=H4App.version()}catch(e){}
if(isApp&&H4App.orientation){$('scOriRow').classList.remove('nope');try{$('scOri').value=H4App.orientation()}catch(e){}$('scOri').onchange=()=>{try{H4App.setOrientation($('scOri').value)}catch(e){}}}
// Jog lock: the jog buttons and the wheel do nothing until the lock button is pressed; 15 s without jogging locks again.
function lockTouch(){if(sc.lock!='1')return;clearTimeout(lockT);lockT=setTimeout(()=>{unlocked=false;lockT=0;ctlJogShow()},15000)}
function lockToggle(){unlocked=!unlocked;clearTimeout(lockT);lockT=0;if(unlocked)lockTouch();ctlJogShow()}
function jogOk(){if(unlocked){lockTouch();return true}msg('Jog is locked: press the lock button first.');return false}
function axesShow(){const u=st.inch?'in':'mm',list=['X','Z'].concat(st.cActive?['C']:[]);
 const val={X:st.x,Z:st.z,C:st.c},mach={X:st.mx,Z:st.mz,C:undefined};
 if($('axes').dataset.n!=list.join()){$('axes').dataset.n=list.join();$('axes').innerHTML=list.map(a=>`<div class="axis" id="ax${a}" style="--c:${AXC[a]}" onclick="selAxis('${a}')"><span class="l">${a}</span>
  <span class="v"><span id="v${a}"></span><small>${a=='C'?'°':u}</small></span><button class="z" onclick="event.stopPropagation();zeroAxis('${a}')">Ø<br>ZERO</button><span class="m" id="m${a}"></span></div>`).join('');
  document.querySelectorAll('#pad [data-a="C"]').forEach(b=>b.hidden=!st.cActive);$('padCl').hidden=!st.cActive}
 for(const a of list){$('v'+a).textContent=(val[a]||0).toFixed(3);$('m'+a).textContent=mach[a]!==undefined?'MCS '+mach[a].toFixed(3):'';$('ax'+a).classList.toggle('sel',a==jogAxis)}}
function selAxis(a){jogAxis=a;ctlJogShow();axesShow()}
function zeroAxis(a){const warn=a=='X'?'\n\nOn a polar machine X0 must stay the chuck centre: zero X only if the tool is exactly there.':'';
 if(confirm('Set '+a+' = 0 here (active work offset)?'+warn))gpMdi('G10 L20 P0 '+a+'0')}
function homeAll(){if(confirm('Home All? The axes move to their switches.'))cmd('home')}
function ctlJogShow(){const wheel=sc.jog=='wheel',list=wheel?STEPS:STEPS.concat(['cont']);
 $('chips').innerHTML=list.map(v=>`<button class="${v==jogStep?'on':''}" onclick="setStep('${v}')">${v=='cont'?'Cont':v<1?String(v).slice(1):v}</button>`).join('')+
  (sc.lock=='1'?`<button class="lk${unlocked?' on':''}" onclick="lockToggle()" title="Jog lock">${unlocked?'&#x1F513;':'&#x1F512;'}</button>`:'');
 $('ctl').classList.toggle('locked',!unlocked);
 const u=jogAxis=='C'?'°':(st.inch?' in':' mm'),su=st.inch?' in':' mm';$('wAxis').textContent=jogAxis;$('wAxis').style.color=AXC[jogAxis];
 $('wStep').textContent=jogStep=='cont'?'':'±'+(+jogStep).toFixed(3)+u;
 $('padC').innerHTML=jogStep=='cont'?'hold to<br>move':'step<br>'+jogStep+su}
function setStep(v){jogStep=v=='cont'?'cont':+v;try{localStorage.setItem('h4step',jogStep)}catch(e){}ctlJogShow()}
function jogDetent(n){if(!jogOk())return;jogPending+=n;jogSend()}
// one jog move at a time: the next one goes when a status read after it shows the machine idle again
async function jogSend(){if(jogBusy||!jogPending||st.running||polls<jogPoll+2)return;const d=+(jogPending*jogStep).toFixed(4);jogPending=0;jogBusy=true;
 const f=jogAxis=='C'?gp.n*gp.c:gp.n;
 try{await post('/mdi',new URLSearchParams({line:'G91 G1 '+jogAxis+d+' F'+f}))}catch(e){msg(e.message);jogPending=0}
 jogPoll=polls;jogBusy=false}
setInterval(jogSend,150);
// Jog buttons: a tap is one step on that axis; with Cont the axis moves while the button is held (as the gamepad:
// /jog every 100 ms, the H4 stops by itself 0.3 s after the last one) and stops when it is released.
let padHeld=null,padTimer=0;
function padStop(){if(!padHeld)return;padHeld.classList.remove('down');padHeld=null;clearInterval(padTimer);let n=3;const s=()=>{gpSend(0,0,0);if(--n>0)setTimeout(s,100)};s()}
document.querySelectorAll('#pad .pb').forEach(b=>{
 b.addEventListener('pointerdown',e=>{e.preventDefault();if(!jogOk())return;const a=b.dataset.a,d=+b.dataset.d;selAxis(a);
  if(jogStep!='cont'){jogDetent(d);b.classList.add('down');setTimeout(()=>b.classList.remove('down'),150);return}
  b.setPointerCapture(e.pointerId);padStop();padHeld=b;b.classList.add('down');
  const v=d*(a=='C'?gp.n*gp.c:gp.n),send=()=>{lockTouch();gpSend(a=='X'?v:0,a=='Z'?v:0,a=='C'?v:0)};send();padTimer=setInterval(send,100)});
 for(const ev of['pointerup','pointercancel','lostpointercapture'])b.addEventListener(ev,padStop);
 b.addEventListener('contextmenu',e=>e.preventDefault())});
addEventListener('blur',padStop);document.addEventListener('visibilitychange',padStop);
(function(){let t='';for(let i=0;i<40;i++){const a=i*9*Math.PI/180,r1=i%5?78:72,x1=100+r1*Math.sin(a),y1=100-r1*Math.cos(a),x2=100+86*Math.sin(a),y2=100-86*Math.cos(a);
 t+=`<line x1="${x1.toFixed(1)}" y1="${y1.toFixed(1)}" x2="${x2.toFixed(1)}" y2="${y2.toFixed(1)}" stroke="${i%5?'var(--dim)':'var(--fg)'}" opacity="${i%5?0.45:0.7}" stroke-width="2"/>`}
 $('ticks').innerHTML=t;const w=$('wheel');let last=null,acc=0;
 const ang=e=>{const r=w.getBoundingClientRect();return Math.atan2(e.clientX-r.left-r.width/2,-(e.clientY-r.top-r.height/2))*180/Math.PI};
 w.addEventListener('pointerdown',e=>{if(!jogOk())return;w.setPointerCapture(e.pointerId);last=ang(e)});
 w.addEventListener('pointermove',e=>{if(last===null)return;const a=ang(e);let d=a-last;if(d>180)d-=360;if(d<-180)d+=360;last=a;acc+=d;wheelAngle+=d;
  $('ticks').setAttribute('transform','rotate('+wheelAngle.toFixed(1)+' 100 100)');while(acc>=9){acc-=9;jogDetent(1)}while(acc<=-9){acc+=9;jogDetent(-1)}});
 for(const ev of['pointerup','pointercancel'])w.addEventListener(ev,()=>{last=null;acc=0});
 $('jp').onclick=()=>jogDetent(1);$('jm').onclick=()=>jogDetent(-1)})();
const MACRO_DEFAULT=[{n:'Unlock ($X)',l:'$X'},{n:'Z up 5 mm',l:'G91 G0 Z5'},{n:'Touch off Z',l:'G10 L20 P0 Z0'},{n:'C to 0°',l:'G90 G0 C0'}];
let macros=MACRO_DEFAULT;try{const m=JSON.parse(localStorage.getItem('h4macros')||'null');if(Array.isArray(m))macros=m}catch(e){}
function macroSheet(){sheet('Macros',`<div class="sgrid" style="grid-template-columns:1fr 1fr">${macros.map((m,i)=>`<button onclick="runMacro(${i})" title="${m.l.replace(/"/g,'&quot;')}">${m.n.replace(/</g,'&lt;')}</button>`).join('')}</div>
 <button onclick="editMacros()">Edit…</button><div class="small">Kept in this phone or browser.</div>`)}
function runMacro(i){const m=macros[i];if(confirm('Run '+m.n+': '+m.l+' ?'))gpMdi(m.l)}
function editMacros(){const t=prompt('One macro per line: name = G-code line (empty = defaults)',macros.map(m=>m.n+' = '+m.l).join(' | '));if(t===null)return;
 macros=t.trim()?t.split('|').map(x=>x.split('=')).filter(p=>p.length>1).map(p=>({n:p[0].trim(),l:p.slice(1).join('=').trim()})):MACRO_DEFAULT;
 try{localStorage.setItem('h4macros',JSON.stringify(macros))}catch(e){}macroSheet()}
function ctlShow(){const s=st.state||'-';$('cState').textContent=s.toUpperCase();$('cState').style.borderColor={Run:'#2e9e5b',Hold:'#d9a21b',Stopping:'#d64040'}[s]||'#2f6db3';
 $('cWcs').textContent=st.wcs||'G54';$('cUnits').textContent=st.inch?'inch':'mm';$('cRssi').textContent=st.rssi?'· '+st.rssi+' dBm':'';
 $('cHold').textContent=s=='Hold'?'RESUME':'HOLD';if(st.fo!==undefined){$('foTxt').textContent=st.fo+'%';$('roTxt').textContent=st.ro+'%';if($('sFo')){$('sFo').textContent=st.fo;$('sRo').textContent=st.ro}}
 axesShow()}
// Pages instead of scrolling: the items of a .pg box that fit are shown, ‹ › in the .pgc after it turns the page.
function pager(box){if(!box.offsetParent)return;const items=[...box.children];items.forEach(i=>i.hidden=false);const H=box.clientHeight;if(!H||!items.length)return;
 const gap=parseFloat(getComputedStyle(box).rowGap)||0,pages=[];let cur=[],h=0;
 for(const it of items){const ih=it.offsetHeight+gap;if(cur.length&&h+ih>H+gap){pages.push(cur);cur=[];h=0}cur.push(it);h+=ih}if(cur.length)pages.push(cur);
 let p=box.dataset.p=='last'?pages.length-1:Math.min(+box.dataset.p||0,pages.length-1);if(box.dataset.p!='last')box.dataset.p=p;
 items.forEach(i=>i.hidden=true);pages[p].forEach(i=>i.hidden=false);
 const c=box.nextElementSibling;if(c&&c.classList.contains('pgc')){c.hidden=pages.length<2;
  c.innerHTML=`<button ${p?'':'disabled'}>&lsaquo;</button><span>${p+1} / ${pages.length}</span><button ${p<pages.length-1?'':'disabled'}>&rsaquo;</button>`;
  const[b1,,b2]=c.children;b1.onclick=()=>{box.dataset.p=p-1;pager(box)};b2.onclick=()=>{box.dataset.p=p+1==pages.length-1&&box.id=='log'?'last':p+1;pager(box)}}}
function pagers(){requestAnimationFrame(()=>document.querySelectorAll('.pg').forEach(pager))}
addEventListener('resize',()=>{pagers();if(curTab=='program')drawPath()});
// Tabs and sub-tabs: one screen each (the phone's Back key goes to the screen before); the last ones are remembered.
const TABS=[['control','Control','<circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="2.5"/><path d="M12 4v3M20 12h-3M12 20v-3M4 12h3"/>'],
 ['program','Program','<path d="M6 3h8l4 4v14H6z"/><path d="M14 3v4h4M9 12h6M9 16h6"/>'],
 ['h4','H4 keys','<rect x="3" y="6" width="18" height="12" rx="3"/><path d="M7 12h4M9 10v4"/><circle cx="15.5" cy="11" r="1"/><circle cx="17.5" cy="13.5" r="1"/>'],
 ['machine','Machine','<path d="M3 12h4l3-7 4 14 3-7h4"/>'],
 ['settings','Settings','<circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M4.9 19.1L7 17M17 7l2.1-2.1"/>']];
const TN=TABS.map(t=>t[0]);let curTab='',subs={},setOpen='';
$('tabs').innerHTML=TABS.map(([t,n,ic])=>`<button data-t="${t}" onclick="tab('${t}')"><svg viewBox="0 0 24 24">${ic}</svg><span>${n}</span></button>`).join('');
try{subs=JSON.parse(localStorage.getItem('h4subs')||'{}')}catch(e){}
function tab(t,noHist){if(!TN.includes(t))t='control';if(t!=curTab){curTab=t;
  document.querySelectorAll('main>section').forEach(s=>s.hidden=s.dataset.tab!=t);
  document.querySelectorAll('#tabs button').forEach(b=>b.classList.toggle('on',b.dataset.t==t));
  if(t=='machine'){logRead=logSeen;tabBadges()}
  try{localStorage.setItem('h4tab',t)}catch(e){}if(!noHist)try{history.pushState({t},'','#'+t)}catch(e){}}
 if(t!='settings')setShow('',true);sub(t,subs[t],true);pagers();if(t=='program')requestAnimationFrame(drawPath)}
function sub(t,p,quiet){const bar=document.querySelector(`.subtabs[data-s="${t}"]`);if(!bar)return;const sec=bar.parentElement;
 const all=[...bar.children].map(b=>b.dataset.p);if(!all.includes(p))p=all[0];subs[t]=p;try{localStorage.setItem('h4subs',JSON.stringify(subs))}catch(e){}
 [...bar.children].forEach(b=>b.classList.toggle('on',b.dataset.p==p));sec.querySelectorAll('.pane').forEach(x=>x.hidden=x.dataset.p!=p);
 if(!quiet&&curTab!=t)tab(t);pagers();if(p=='path')requestAnimationFrame(drawPath)}
document.querySelectorAll('.subtabs button').forEach(b=>b.onclick=()=>sub(b.parentElement.dataset.s,b.dataset.p));
// Settings: a list; a page opens on its own (Back closes it), or next to the list on a wide screen.
function setList(){const L=[['look','Appearance'],['screen','Screen and jog']].concat(cfgGroups.map((g,i)=>['m'+i,'Machine: '+g])).concat([['wifi','WiFi'],['fw','Firmware']]).concat(isApp&&H4App.checkAppUpdate?[['app','App update']]:[]);
 $('slist').innerHTML=L.map(([k,n])=>`<button data-k="${k}" onclick="setShow('${k}')"><span>${n}</span><span>&rsaquo;</span></button>`).join('');setShow(setOpen,true)}
function setShow(k,noHist){const wide=matchMedia('(orientation:landscape) and (min-width:700px)').matches;if(!k&&wide)k='look';
 if(k&&!noHist&&!wide&&!setOpen)try{history.pushState({t:'settings',s:k},'','#settings')}catch(e){}
 setOpen=k;$('sets').classList.toggle('open',!!k);document.querySelectorAll('.spage').forEach(p=>p.hidden=p.dataset.sp!=k);
 document.querySelectorAll('#slist button').forEach(b=>b.classList.toggle('on',b.dataset.k==k));
 const b=document.querySelector(`#slist button[data-k="${k}"]`);$('sTitle').textContent=b?b.firstChild.textContent:'';pagers()}
addEventListener('popstate',e=>{const s=e.state||{};sheetClose();if(s.s)setShow(s.s,true);else setShow('',true);tab(s.t||location.hash.slice(1)||'control',true)});
let logRead=-1;function tabBadges(){const b=q=>document.querySelector('#tabs button[data-t="'+q+'"]');
 const m=b('machine'),p=b('program');let d=m.querySelector('.dot');const unread=logRead>=0&&st.log!==undefined&&st.log>logRead&&curTab!='machine';
 if(unread&&!d){d=document.createElement('i');d.className='dot';m.appendChild(d)}if(!unread&&d)d.remove();
 let c=p.querySelector('.pct');const run=st.running&&st.progress!==undefined;if(run&&!c){c=document.createElement('i');c.className='pct';p.appendChild(c)}
 if(run)c.textContent=Math.floor(st.progress*100)+'%';else if(c)c.remove()}
// swipe left / right changes the screen (never on the jog buttons, the wheel, the drawing, fields or the H4 keys)
(function(){let x0=null,y0=0,t0=0;addEventListener('touchstart',e=>{const t=e.target;
 x0=t.closest('#pad,.wheelbox,canvas,input,select,textarea,.keys,.lcd,.chips,.sheet')?null:e.touches[0].clientX;y0=e.touches[0].clientY;t0=Date.now()},{passive:true});
 addEventListener('touchend',e=>{if(x0===null)return;const dx=e.changedTouches[0].clientX-x0,dy=e.changedTouches[0].clientY-y0;x0=null;
  if(Date.now()-t0<600&&Math.abs(dx)>80&&Math.abs(dx)>2*Math.abs(dy)){const i=TN.indexOf(curTab)+(dx<0?1:-1);if(i>=0&&i<TN.length)tab(TN[i])}},{passive:true})})();
if(DEMO)$('demoTag').hidden=false;
scApply();
{let t0=location.hash.slice(1);if(!TN.includes(t0))try{t0=localStorage.getItem('h4tab')}catch(e){}t0=TN.includes(t0)?t0:'control';
 tab(t0,true);try{history.replaceState({t:t0},'','#'+t0)}catch(e){}}
setList();listFiles();loadCfg();poll();
</script></body></html>
)rawpage";
