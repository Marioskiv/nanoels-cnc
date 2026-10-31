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
.tabs{grid-area:t;display:grid;grid-template-columns:repeat(6,1fr);background:var(--card);border-top:1px solid var(--line);padding-bottom:env(safe-area-inset-bottom)}
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

/* Lathe screen (the H4's own modes): operations on top, drawing beside or above the DRO and the settings */
.lt{display:grid!important;grid-template-rows:auto minmax(0,1fr) auto;gap:8px}
.ltops{display:grid;grid-template-columns:repeat(8,minmax(0,1fr));gap:6px}
.ltop{display:flex;flex-direction:column;align-items:center;gap:2px;padding:6px 2px 5px;border-radius:10px;border:1px solid var(--line);background:var(--card);color:var(--fg);min-width:0;cursor:pointer}
.ltop.sel{border-color:var(--or);border-style:dashed}.ltop:disabled{opacity:.55;cursor:default}
.ltop svg{width:30px;height:22px}.ltop span{font-size:12px;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:100%}
.ltop.on{border-color:var(--or);box-shadow:inset 0 -3px 0 var(--or);background:var(--tile)}
.ltop .ic-l{stroke:var(--fg);fill:none}.ltop .ic-c{fill:#ff7a2e}.ltop .ic-s{fill:#8b95a6}
.ltmain{display:grid;grid-template-columns:minmax(0,.85fr) minmax(0,1.15fr);gap:8px;min-height:0}
.ltview{background:var(--tile);border:1px solid var(--tileb);border-radius:12px;min-height:0;display:flex;flex-direction:column;overflow:hidden}
.ltview svg{flex:1;min-height:0;width:100%}
.ltleg{display:flex;gap:12px;flex-wrap:wrap;padding:5px 10px;border-top:1px solid var(--tileb);font-size:12px;color:var(--dim)}
.ltleg i{display:inline-block;width:12px;height:4px;border-radius:2px;vertical-align:middle;margin-right:4px}
.ltside{display:grid;grid-template-rows:auto minmax(0,1fr);gap:8px;min-height:0}
.ltdro{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.ltax{grid-column:1/-1;display:grid;grid-template-columns:auto minmax(0,1fr);align-items:center;gap:0 12px;padding:5px 10px 5px 12px;border-radius:12px;
 background:var(--tile);border:1px solid var(--tileb);border-left:7px solid var(--c);box-shadow:0 0 18px -10px var(--c)}
.ltax .l{font:900 clamp(32px,4.6vw,50px)/1 system-ui;color:var(--c)}
.ltax .v{font:600 clamp(28px,4.3vw,46px)/1.05 ui-monospace,monospace;text-align:right;white-space:nowrap;overflow:hidden;font-variant-numeric:tabular-nums}
.ltax .v small,.ltsp .v small{font:500 12px system-ui;color:var(--dim);margin-left:5px}
.ltsp{display:grid;grid-template-columns:auto 1fr;align-items:baseline;gap:6px;background:var(--tile);border:1px solid var(--tileb);border-radius:12px;padding:5px 10px}
.ltsp b{font-size:12px;color:var(--dim);font-weight:600}.ltsp .v{font:600 clamp(18px,2.4vw,24px) ui-monospace,monospace;text-align:right;white-space:nowrap}
.ltpanel h3{margin:0;font-size:16px}
.card.ltpanel{overflow-y:auto;min-height:0}
.ltfields{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:8px}.ltfields .f{display:flex;flex-direction:column;gap:3px;min-width:0}.ltfields .wide{grid-column:1/-1}
.ltfields label{font-size:12px;color:var(--dim)}
.ltfields .step,.ltg .step{display:grid;grid-template-columns:36px 1fr 36px;border:1px solid var(--line);border-radius:9px;overflow:hidden;background:var(--tile)}
.ltfields .step button,.ltg .step button{border:0;background:transparent;font-size:20px;padding:0;color:var(--fg)}
.ltfields .step .numv,.ltg .step .numv{border:0;border-left:1px solid var(--line);border-right:1px solid var(--line);border-radius:0;background:transparent;color:var(--fg);text-align:center;font:600 18px ui-monospace,monospace;width:100%;min-width:0;padding:6px 0;cursor:text}
.ltzero{display:flex;flex-wrap:wrap;align-items:center;gap:6px;font-size:13px}.ltzero>button{border:1px solid var(--line);border-radius:9px;background:var(--tile);color:var(--fg);padding:6px 10px;font-weight:700}
.ltzero .ltxin{margin-left:auto;color:var(--dim)}.ltzero .seg{display:flex;border:1px solid var(--line);border-radius:9px;overflow:hidden}.ltzero .seg button{border:0;border-radius:0;background:var(--tile);color:var(--fg);padding:6px 8px;font-size:12px}
.ltzero .seg button.on{background:var(--or);color:#111;font-weight:700}.ltturn{font-size:14px;color:var(--dim);font-weight:600}
.lthelpb{margin-left:2px}.lthelp{font-size:12.5px;line-height:1.5;color:var(--fg);background:var(--tile);border:1px solid var(--line);border-radius:10px;padding:8px 10px}
.ltgroups{display:grid;grid-template-columns:1fr 1fr;gap:8px}.ltg{border:1px solid var(--line);border-left:4px solid var(--c,var(--dim));border-radius:10px;padding:6px 8px;display:flex;flex-direction:column;gap:6px;min-width:0}
.ltg h4{margin:0;font-size:12.5px;color:var(--c,var(--dim));font-weight:800}.ltgz .ltgf,.ltgx .ltgf{display:flex;flex-direction:column;gap:6px}
.ltgc .ltgf{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:6px}.ltg .f{display:flex;flex-direction:column;gap:3px;min-width:0}.ltg label{font-size:12px;color:var(--dim)}
.ltpad{position:fixed;inset:0;z-index:50;background:rgba(0,0,0,.55);display:flex;align-items:center;justify-content:center;padding:16px}
.ltpadb{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;width:min(340px,100%);display:flex;flex-direction:column;gap:10px}
.ltpadt{font-weight:700;font-size:15px}.ltpadv{font:700 30px ui-monospace,monospace;text-align:right;background:var(--tile);border:1px solid var(--line);border-radius:10px;padding:8px 12px;min-height:1.3em}
.ltpadk{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}.ltpadk button{border:1px solid var(--line);border-radius:10px;background:var(--tile);color:var(--fg);font:700 22px system-ui;padding:12px 0}
.ltpadk button.ok{background:var(--ok);color:#fff;border:0;grid-column:1/-1}
.ltfields .seg,.ltg .seg{display:flex;border:1px solid var(--line);border-radius:9px;overflow:hidden}
.ltfields .seg button,.ltg .seg button{flex:1;border:0;border-radius:0;background:var(--tile);padding:7px 4px;font-size:13px;color:var(--fg)}
.ltfields .seg button.on,.ltg .seg button.on{background:var(--or);color:#111;font-weight:700}
.ltfields .sel{border:1px solid var(--line);border-radius:9px;background:var(--tile);color:var(--fg);padding:7px 8px;font:600 14px system-ui;width:100%}
.ltinfo{font-size:12.5px;color:var(--dim);line-height:1.45}.ltinfo .bad{color:var(--bad)}
.ltnow{font:11.5px/1.35 ui-monospace,monospace;color:var(--dim);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.ltbot{display:grid;grid-template-columns:auto 1fr;gap:10px;align-items:center}
.ltstart{position:relative;overflow:hidden;border:0;border-radius:12px;background:var(--ok);color:#fff;font-weight:800;letter-spacing:.04em;padding:13px 22px;min-width:190px}
.ltstart .fill{position:absolute;inset:0;width:0;background:rgba(255,255,255,.3)}.ltstart span{position:relative}.ltstart.run{background:var(--acc)}
.ltprog{display:flex;flex-direction:column;gap:4px;min-width:0}.ltprog .bar{height:8px;border-radius:99px;background:var(--tile);border:1px solid var(--line);overflow:hidden}
.ltprog .bar div{height:100%;width:0;background:var(--or)}.ltprog small{color:var(--dim);font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.ltrows{display:grid;grid-template-columns:auto 1fr;gap:4px 12px;font-size:14px}
.ltrows span:nth-child(odd){color:var(--dim)}.ltrows span:nth-child(even){font:600 15px ui-monospace,monospace;text-align:right}
.ltnote{margin-top:auto;font-size:12px;color:var(--dim)}
@media (orientation:landscape) and (max-height:860px) and (min-width:761px){.ltax{grid-column:auto;padding:3px 8px}.ltax .l{font-size:clamp(26px,3vw,36px)}.ltax .v{font-size:clamp(20px,2.5vw,32px)}.ltsp{padding:3px 8px}}
@media (orientation:portrait),(max-width:760px){.ltops{grid-template-columns:repeat(4,minmax(0,1fr))}.ltmain{grid-template-columns:1fr;grid-template-rows:minmax(80px,24%) minmax(0,1fr)}
 .ltleg{display:none}.ltax .l{font-size:28px}.ltax .v{font-size:25px}.ltpanel h3{font-size:14px}.ltmain{grid-template-rows:minmax(70px,18%) minmax(0,1fr)}
 .ltg .step .numv{font-size:15px;padding:3px 0}.ltg .step{grid-template-columns:30px 1fr 30px}.ltgroups{gap:5px}.ltg{padding:5px 6px}.ltg label{font-size:11px}
 .ltfields{gap:5px}.ltfields label{font-size:11px}.ltfields .step .numv{font-size:15px;padding:3px 0}.ltfields .step{grid-template-columns:30px 1fr 30px}
 .ltfields .seg button{padding:5px 2px;font-size:12px}.ltfields .sel{padding:4px 6px}.ltinfo{font-size:11.5px}.ltnow{font-size:11px}.ltstart{min-width:150px;padding:11px 12px}
 .ltdro{gap:4px}.ltsp{padding:3px 8px}.ltsp .v{font-size:17px}.ltside{gap:6px}.card.ltpanel{padding:8px;gap:6px}}
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

<section class="scr lt" data-tab="lathe" hidden>
<div class="ltops" id="ltOps"></div>
<div class="ltmain"><div class="ltview"><svg id="ltSvg" viewBox="0 0 600 300" preserveAspectRatio="xMidYMid meet"></svg><div class="ltleg" id="ltLeg"></div></div>
<div class="ltside"><div class="ltdro">
<div class="ltax" id="ltXb"><span class="l">X</span><span class="v"><span id="ltXv">-</span><small id="ltXu"></small></span></div>
<div class="ltax" id="ltZb"><span class="l">Z</span><span class="v"><span id="ltZv">-</span><small id="ltZu"></small></span></div>
<div class="ltsp"><b id="ltRpmL"></b><span class="v"><span id="ltRpm">-</span><small id="ltRpmU"></small></span></div>
<div class="ltsp"><b id="ltAngL"></b><span class="v" id="ltAng">-</span></div></div>
<div class="card ltpanel" id="ltForm"></div></div></div>
<div class="ltbot"><button type="button" class="ltstart" id="ltStart"><span class="fill" id="ltFill"></span><span id="ltStartT"></span></button>
<div class="ltprog"><div class="bar"><div id="ltBar"></div></div><small id="ltProg"></small><small class="ltnow" id="ltNow"></small></div></div>
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
<div class="fr"><label for="lkLang">Language / Γλώσσα</label><select id="lkLang"><option value="en">English</option><option value="el">Ελληνικά</option></select></div>
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
 const S={x:-21,z:-2,c:0,fo:100,ro:100,state:'Idle',running:false,paused:false,progress:0,homed:[0,0,0],jog:{x:0,z:0,c:0,t:0},
  log:['Demo: a simulated machine, no H4 is connected.'],t:Date.now(),benchN:0,bench:'',program:'',steps:0,
  files:[{name:'demo_spiral.ngc'},{name:'demo_F_test.ngc'}],ang:0,mode:4,on:false,lt:{pitch:0.1,measure:0,starts:1,passes:8,cone:1,aux:1,setup:0,op:0,zl:40,zr:0,xl:-18,xr:-20}};
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
   if(S.progress>=1){S.running=false;S.state='Idle';S.z=5;add('Demo: program end')}}
  if(S.on){const L=S.lt;S.lp=(S.lp||0)+dt/3;if(S.lp>=1){S.lp=0;L.op++;if(L.op>=L.passes){S.on=false;L.op=0;add('Demo: lathe passes done')}}
   if(L.zl!=null&&L.zr!=null)S.z=L.zr+(L.zl-L.zr)*(S.lp||0)}}
 function status(){tick();const st=S.running?(S.paused?'Hold':'Run'):'Idle';
  return {state:st,gcodeMode:1,on:S.on||S.running?1:0,text:S.paused?'Paused':'',x:S.x,z:S.z,c:S.c,cActive:1,inch:0,feed:S.running?60*S.fo/100:0,rpm:0,
   line:S.running?lines[Math.min(lines.length-1,6+Math.floor(S.progress*pts.length))]:'',progress:S.progress,program:S.program,
   programLoaded:S.program?1:0,underruns:0,estop:0,running:S.running?1:0,fsTotal:10e6,fsUsed:4200,psram:2e6,heap:150000,
   wifi:'DEMO - no H4 connected, nothing moves',lcd:['DEMO','X '+S.x.toFixed(3),'Z '+S.z.toFixed(3),'C '+S.c.toFixed(2)],
   mode:S.running?9:S.mode,rpm:S.running?0:820,lathe:Object.assign({},S.lt,{ang:(S.ang=(S.ang+29)%360)}),
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
  if(p=='/lathe'){if(S.on||S.running)return no('The machine is ON or moving: stop it (OFF) first.');
   if(arg('do')=='z0'){S.z=0;add('Demo: Z0');return ok('Z0')}if(arg('do')=='x0'){S.x=0;add('Demo: X0');return ok('X0')}
   if(arg('do')=='dia'){S.x=-(+arg('d'))/2;add('Demo: X0 from Ø '+arg('d'));return ok('X0')}
   if(arg('do')=='start'){if(S.mode==9)return no('G-code mode: use the G-code tab.');S.on=true;S.lp=0;S.lt.op=0;add('Demo: lathe ON');return ok('ON')}
   const L=S.lt,n=k=>arg(k)===null?undefined:(arg(k)===''?null:+arg(k));
   if(n('mode')!==undefined)S.mode=n('mode');if(n('pitch')!==undefined)L.pitch=n('pitch');for(const k of ['starts','passes','cone','aux'])if(n(k)!==undefined)L[k]=n(k);
   for(const k of ['zl','zr','xl','xr'])if(n(k)!==undefined)L[k]=n(k);add('Demo: lathe set '+u.search);return ok('Set')}
  if(p=='/delete'){S.files=S.files.filter(f=>f.name!=arg('name'));return ok('Deleted')}
  if(p=='/mdi'){const l=(arg('line')||'').trim();if(S.running)return no('The machine is ON or moving: stop it (OFF) first.');
   if(/^\$H$/i.test(l)){S.homed=[1,1,1];S.x=0;S.z=0;add('Demo: Home All');return ok('Home All')}
   const m=/G91 G1 ([XZC])([-\d.]+)/i.exec(l);if(m){S[m[1].toLowerCase()]+= +m[2]}
   add('Demo: '+l);return ok('Running: '+l)}
  if(p=='/cmd'){const d=arg('do');
   if(d=='start'){if(S.running)return no('The machine is ON or moving: stop it (OFF) first.');S.program=arg('name');S.running=true;S.paused=false;S.progress=0;add('Demo: start '+S.program);return ok('Started '+S.program)}
   if(d=='stop'){S.running=false;S.on=false;S.paused=false;add('Demo: STOP');return ok('Stopping')}
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
function startSel(){if(!selected){msg('Select a file first (G-code, Files).');return}
 if(confirm('Start '+selected+' on the machine?'))cmd('start','&name='+encodeURIComponent(selected))}
let polls=0;async function poll(){try{const r=await fetch('/status');st=await r.json();polls++;show()}catch(e){$('cState').textContent='NO CONNECTION'}
 setTimeout(poll,curTab=='lathe'?150:350)}
function ovr(c){cmd('ovr','&code='+c)}
let benchSeen=-1;function bench(n){cmd('bench','&name='+encodeURIComponent(n))}
function diagShow(d){if(!d)return;const per=d.rate?1e6/d.rate:0;
 const ok=d.late==0&&(!d.rate||d.isrUs<0.7*per)&&!st.underruns;
 $('diag').innerHTML=(ok?'<b>OK</b>':'<b style="color:#d64040">CHECK</b>')+'<br>Step interrupt, longest: '+d.isrUs+' µs'+(d.rate?' (time between steps at the highest rate: '+per.toFixed(1)+' µs)':'')+
  '<br>Highest step rate: '+d.rate+' /s (limit '+d.limit+' /s)<br>Started late (over 10 µs): '+d.late+' times, latest '+d.lateUs+' µs<br>Step buffer ran empty: '+st.underruns+'<br>Steps given: '+d.steps}
function show(){const u=st.inch?' in':' mm';diagShow(st.diag);ctlShow();if(curTab=='lathe')ltShow();if(st.fw)$('fwVer').textContent=st.fw;
 if(st.benchN!==undefined){if(benchSeen>=0&&st.benchN!=benchSeen)msg(st.bench);benchSeen=st.benchN}if(st.fo!==undefined){$('fo').textContent=st.fo+(st.ovrOff?' (M49: not applied)':'');$('ro').textContent=st.ro+(st.ovrOff?' (M49: not applied)':'')}
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
const LOOK0={lang:'en',theme:'dark',acc:'#ff9a1f',X:'#ff9a1f',Z:'#7ad151',C:'#e24bd0',size:'34'};let look=Object.assign({},LOOK0);
try{Object.assign(look,JSON.parse(localStorage.getItem('h4look')||'{}'))}catch(e){}
const AXC={X:look.X,Z:look.Z,C:look.C};
// Appearance: theme, accent and axis colours, number size (stored in the browser)
function lookApply(){document.body.classList.toggle('light',look.theme=='light');const r=document.documentElement.style;
 r.setProperty('--or',look.acc);r.setProperty('--dro',look.size+'px');AXC.X=look.X;AXC.Z=look.Z;AXC.C=look.C;
 document.querySelectorAll('#pad [data-a]').forEach(b=>b.style.setProperty('--c',AXC[b.dataset.a]));$('padCl').style.setProperty('--c',AXC.C);
 const a=$('axes');if(a)a.dataset.n='';if(window.ltReady)ltLang();if(typeof ctlJogShow=='function'&&$('chips'))ctlJogShow()}
function lookSave(){try{localStorage.setItem('h4look',JSON.stringify(look))}catch(e){}lookApply()}
function lookReset(){look=Object.assign({},LOOK0);lookForm();lookSave()}
function lookForm(){$('lkLang').value=look.lang;$('lkTheme').value=look.theme;$('lkAcc').value=look.acc;$('lkX').value=look.X;$('lkZ').value=look.Z;$('lkC').value=look.C;$('lkSize').value=look.size}
lookForm();for(const [id,k] of [['lkLang','lang'],['lkTheme','theme'],['lkAcc','acc'],['lkX','X'],['lkZ','Z'],['lkC','C'],['lkSize','size']])
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
 $('cWcs').textContent=st.wcs||'G54';$('cUnits').textContent=(st.inch?'inch':'mm')+(st.tool?' · T'+st.tool:'')+(st.spin==3||st.spin==4?' · S'+st.sp+' M'+st.spin:'')+(st.cool?' · '+st.cool:'');$('cRssi').textContent=st.rssi?'· '+st.rssi+' dBm':'';
 $('cHold').textContent=s=='Hold'?'RESUME':'HOLD';if(st.fo!==undefined){$('foTxt').textContent=st.ovrOff?'off':st.fo+'%';$('roTxt').textContent=st.ovrOff?'off':st.ro+'%';if($('sFo')){$('sFo').textContent=st.fo;$('sRo').textContent=st.ro}}
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
 ['lathe','Lathe','<path d="M3 8h4v8H3zM7 10h11v4H7z"/><path d="M18 9l3 3-3 3"/>'],
 ['program','G-code','<path d="M6 3h8l4 4v14H6z"/><path d="M14 3v4h4M9 12h6M9 16h6"/>'],
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

// ---------------------------------------------------------------- Lathe screen: the H4's own modes.
// As on the H4 keypad, work is set from the part's zero: Z0 on the face, X0 on the surface (values are depths) or
// on the centre line (values are diameters, X0 set from a measured diameter). The form is only a plan: typing
// moves nothing and sends nothing. HOLD TO START sends it as the H4's own values (pitch, starts, passes, cone
// ratio, external / internal, the four stops) through /lathe, which uses the same functions as the H4's keys,
// then presses ON through the H4's key queue: the H4 itself moves the tool to the start and runs the mode.
// H4 conventions (as its display): Z grows to the left (towards the chuck); X grows towards the centre, so with
// X0 on the centre line the outside of the part is negative (X = -Ø/2).
const LTT={en:{tabs:{control:'Control',lathe:'Lathe',program:'G-code',h4:'H4 keys',machine:'Machine',settings:'Settings'},
  rpm:'Spindle',rpmu:'rpm',ang:'Spindle angle',turning:'turning',now:'On the H4 now',start:'HOLD TO START',running:'RUNNING',
  inch:'The lathe screen works in mm: set the H4 to mm.',gcode:'The H4 is in G-code mode: choose a mode above to switch it to the lathe.',
  pass:'pass',passes:'passes',ready:'ready',estimate:'at',stopped:'time: start the spindle to see it',
  ext:'External',int:'Internal',none:'—',legend:['stock','material removed','Z0 and stops','tool'],
  step1:'1 · Zero on the part',step2:'2 · ',z0:'Z0 here',x0:'X0 here',dia:'Ø → X0',help:'How?',xin:'X in',xdia:'diameters',xdep:'depth',
  helpText:'<b>Z0</b>: touch the face of the part with the tool tip (or take a light facing cut) and press <b>Z0 here</b>.<br><b>X, way 1 (diameters, recommended)</b>: take a light cut on the diameter, do not move X, measure the diameter and press <b>Ø → X0</b> with that number. The H4 then knows where the centre is and every X value is a diameter.<br><b>X, way 2 (depth)</b>: touch the diameter and press <b>X0 here</b>; X values are then how deep you cut.<br>The same as the H4 keys Z, X and A (diameter on the numpad, then A). After a tool change: zero again.',
  zx:'Z (length, along the part)',xx:'X (diameter)',xxd:'X (depth)',cx:'Cut',
  diaAsk:'Measured diameter (mm)',zeroDone:'Zero set.',busy:'Stop the H4 first (OFF).',
  goTo:'At START the H4 moves the tool to Z {z}, X {x} by itself, then cuts.',fromTool:'Starts where the tool stands at START.',
  coneStop:'Stops after {l} mm of Z.',cutZ:'Cuts at the Z where the tool stands now: Z {z}.',x0note:'X in diameters: X0 must be on the centre (Ø → X0).',
  err:{same:'Nothing to cut: the start and the end are the same.',dia:'A diameter cannot be negative.',cone:'Ø large must be larger than Ø small.',side:'For this side the final Ø must be {w} than the part Ø.',smaller:'smaller',larger:'larger',val:'Fill every value.',len:'The length must be more than 0.'},
  ok:'OK',cancel:'Cancel',clear:'Empty',
  m:{0:'Feed',2:'Constant feed',3:'Taper',4:'Turning',5:'Facing',6:'Parting',7:'Thread',8:'Sphere',9:'G-code'},
  t:{0:'Feed per turn',2:'Constant feed (without the spindle)',3:'Taper',4:'Turning in passes',5:'Facing in passes',6:'Parting / grooving',7:'Threading',8:'Half sphere / ellipse'},
  f:{feed:'Feed mm/turn',pitch:'Pitch mm',speed:'Speed mm/s',zstop:'Stop at Z (empty: none)',za:'Start Z (0 = face)',len:'Length mm',zdep:'Depth into the face mm',rz:'Radius Z mm',
   dstock:'Part Ø mm',dfin:'Final Ø mm',depth:'Depth mm',xin:'How far in mm',dfinf:'To Ø mm (0 = centre)',dfinp:'To Ø mm (0 = cut off)',dbig:'Ø large mm',dsmall:'Ø small mm',
   dnom:'Thread Ø mm',dbore:'Hole Ø mm',tdep:'Thread depth mm',rx:'Radius X mm',
   passes:'Passes',starts:'Starts',dir:'Direction',hand:'Hand',side:'Side',shape:'Shape',
   preset:'Standard',left:'← to the chuck',right:'→ away',rh:'Right',lh:'Left',convex:'Convex',concave:'Concave',ext:'External',int:'Internal'}},
 el:{tabs:{control:'Έλεγχος',lathe:'Τόρνος',program:'G-code',h4:'Πλήκτρα H4',machine:'Μηχάνημα',settings:'Ρυθμίσεις'},
  rpm:'Στροφές',rpmu:'σ.α.λ.',ang:'Γωνία ατράκτου',turning:'γυρίζει',now:'Τώρα στο H4',start:'ΚΡΑΤΑ ΓΙΑ ΕΚΚΙΝΗΣΗ',running:'ΣΕ ΕΞΕΛΙΞΗ',
  inch:'Η οθόνη τόρνου δουλεύει σε mm: γύρισε το H4 σε mm.',gcode:'Το H4 είναι σε G-code: διάλεξε λειτουργία επάνω για να γυρίσει σε τόρνο.',
  pass:'πέρασμα',passes:'περάσματα',ready:'έτοιμο',estimate:'στις',stopped:'χρόνος: βάλε μπρος την άτρακτο για να φανεί',
  ext:'Εξωτερικό',int:'Εσωτερικό',none:'—',legend:['κομμάτι','υλικό που φεύγει','Z0 και όρια','εργαλείο'],
  step1:'1 · Μηδέν στο κομμάτι',step2:'2 · ',z0:'Z0 εδώ',x0:'X0 εδώ',dia:'Ø → X0',help:'Πώς;',xin:'X σε',xdia:'διαμέτρους',xdep:'βάθος',
  helpText:'<b>Z0</b>: άγγιξε με τη μύτη του κοπτικού το πρόσωπο του κομματιού (ή πάρε ένα λεπτό πάσο προσώπου) και πάτα <b>Z0 εδώ</b>.<br><b>X, τρόπος 1 (διάμετροι, προτείνεται)</b>: πάρε ένα λεπτό πάσο στη διάμετρο, μην κουνήσεις τον X, μέτρησε τη διάμετρο με το παχύμετρο και πάτα <b>Ø → X0</b> γράφοντας τον αριθμό. Το H4 ξέρει τότε πού είναι το κέντρο και κάθε τιμή X είναι διάμετρος.<br><b>X, τρόπος 2 (βάθος)</b>: άγγιξε τη διάμετρο και πάτα <b>X0 εδώ</b>· οι τιμές X είναι τότε πόσο βαθιά κόβεις.<br>Ίδιο με τα πλήκτρα Z, X και A του H4 (διάμετρος στο αριθμητικό, μετά A). Μετά από αλλαγή κοπτικού: ξανά μηδέν.',
  zx:'Z (μήκος, κατά μήκος του κομματιού)',xx:'X (διάμετρος)',xxd:'X (βάθος)',cx:'Κοπή',
  diaAsk:'Μετρημένη διάμετρος (mm)',zeroDone:'Το μηδέν ορίστηκε.',busy:'Σταμάτησε πρώτα το H4 (OFF).',
  goTo:'Στην ΕΚΚΙΝΗΣΗ το H4 πηγαίνει μόνο του το εργαλείο στο Z {z}, X {x} και κόβει.',fromTool:'Ξεκινά από εκεί που είναι το εργαλείο στην ΕΚΚΙΝΗΣΗ.',
  coneStop:'Σταματά μετά από {l} mm στον Z.',cutZ:'Κόβει στο Z όπου στέκεται τώρα το εργαλείο: Z {z}.',x0note:'X σε διαμέτρους: το X0 πρέπει να είναι στο κέντρο (Ø → X0).',
  err:{same:'Δεν έχει τίποτα να κόψει: η αρχή και το τέλος είναι ίδια.',dia:'Η διάμετρος δεν μπορεί να είναι αρνητική.',cone:'Η μεγάλη Ø πρέπει να είναι μεγαλύτερη από τη μικρή.',side:'Γι αυτή την πλευρά η τελική Ø πρέπει να είναι {w} από την Ø του κομματιού.',smaller:'μικρότερη',larger:'μεγαλύτερη',val:'Συμπλήρωσε όλες τις τιμές.',len:'Το μήκος πρέπει να είναι πάνω από 0.'},
  ok:'OK',cancel:'Άκυρο',clear:'Κενό',
  m:{0:'Πρόωση',2:'Σταθερή πρόωση',3:'Κώνος',4:'Τόρνευση',5:'Πρόσωπο',6:'Αποκοπή',7:'Σπείρωμα',8:'Σφαίρα',9:'G-code'},
  t:{0:'Πρόωση ανά στροφή',2:'Σταθερή πρόωση (χωρίς άτρακτο)',3:'Κώνος',4:'Τόρνευση σε περάσματα',5:'Τόρνευση προσώπου',6:'Αποκοπή / αυλάκι',7:'Σπείρωμα',8:'Ημισφαίριο / έλλειψη'},
  f:{feed:'Πρόωση mm/στροφή',pitch:'Βήμα mm',speed:'Ταχύτητα mm/s',zstop:'Στάση στο Z (κενό: καμία)',za:'Αρχή Z (0 = πρόσωπο)',len:'Μήκος mm',zdep:'Βάθος στο πρόσωπο mm',rz:'Ακτίνα Z mm',
   dstock:'Ø κομματιού mm',dfin:'Τελική Ø mm',depth:'Βάθος mm',xin:'Πόσο μέσα mm',dfinf:'Έως Ø mm (0 = κέντρο)',dfinp:'Έως Ø mm (0 = αποκοπή)',dbig:'Ø μεγάλη mm',dsmall:'Ø μικρή mm',
   dnom:'Ø σπειρώματος mm',dbore:'Ø τρύπας mm',tdep:'Βάθος σπειρώματος mm',rx:'Ακτίνα X mm',
   passes:'Περάσματα',starts:'Αρχές',dir:'Φορά',hand:'Σπείρωμα',side:'Πλευρά',shape:'Μορφή',
   preset:'Τυποποιημένο',left:'← προς το τσοκ',right:'→ από το τσοκ',rh:'Δεξιό',lh:'Αριστερό',convex:'Κυρτό',concave:'Κοίλο',ext:'Εξωτερικό',int:'Εσωτερικό'}}};
const LTOPS=[[0,'<rect class="ic-s" x="2" y="8" width="24" height="9" rx="1"/><path class="ic-l" d="M25 4 L6 4 M10 1 L6 4 L10 7" stroke-width="2"/>'],
 [4,'<rect class="ic-s" x="2" y="9" width="24" height="10" rx="1"/><rect class="ic-c" x="12" y="6" width="14" height="3"/><path class="ic-l" d="M27 3 L12 3" stroke-width="1.6"/>'],
 [5,'<rect class="ic-s" x="2" y="4" width="20" height="16" rx="1"/><rect class="ic-c" x="22" y="4" width="4" height="16"/><path class="ic-l" d="M28 2 L28 21" stroke-width="1.6"/>'],
 [3,'<path class="ic-s" d="M2 7 L14 7 L26 13 L26 19 L2 19 Z"/><path class="ic-c" d="M14 4 L26 4 L26 13 Z"/>'],
 [7,'<rect class="ic-s" x="2" y="10" width="24" height="10" rx="1"/><path class="ic-c" d="M8 10 L10 6 L12 10 L14 6 L16 10 L18 6 L20 10 L22 6 L24 10 Z"/>'],
 [6,'<rect class="ic-s" x="2" y="6" width="24" height="14" rx="1"/><rect class="ic-c" x="15" y="6" width="3" height="10"/><path class="ic-l" d="M16.5 1 L16.5 5" stroke-width="2"/>'],
 [8,'<path class="ic-s" d="M2 7 L14 7 A10 10 0 0 1 24 15 L24 19 L2 19 Z"/><path class="ic-c" d="M14 7 A10 10 0 0 1 24 15 L24 4 L14 4 Z"/>'],
 [2,'<rect class="ic-s" x="2" y="9" width="24" height="9" rx="1"/><path class="ic-l" d="M6 4 L24 4 M19 1 L24 4 L19 7" stroke-width="1.8"/><circle class="ic-c" cx="6" cy="4" r="2"/>']];
// Ready-made values; every field stays free to change. Threads: ISO metric coarse / fine and UNC as
// [name, nominal Ø, pitch]; external depth 0.6134 P, internal 0.5413 P from the minor Ø D - 1.0825 P.
// Tapers: diameter change per mm of length (Morse, metric 1:20, 7:24, 1:10, taper pins 1:50). Spheres: Ø.
const LTPRE={7:[['M3 × 0.5',3,0.5],['M4 × 0.7',4,0.7],['M5 × 0.8',5,0.8],['M6 × 1',6,1],['M8 × 1.25',8,1.25],['M8 × 1',8,1],['M10 × 1.5',10,1.5],['M10 × 1.25',10,1.25],
  ['M12 × 1.75',12,1.75],['M12 × 1.5',12,1.5],['M12 × 1.25',12,1.25],['M14 × 2',14,2],['M14 × 1.5',14,1.5],['M16 × 2',16,2],['M16 × 1.5',16,1.5],['M20 × 2.5',20,2.5],['M20 × 1.5',20,1.5],
  ['M24 × 3',24,3],['M24 × 2',24,2],['1/4-20 UNC',6.35,1.27],['5/16-18 UNC',7.9375,25.4/18],['3/8-16 UNC',9.525,25.4/16],['1/2-13 UNC',12.7,25.4/13],['5/8-11 UNC',15.875,25.4/11],['3/4-10 UNC',19.05,2.54]],
 3:[['MT0',0.05205],['MT1',0.04988],['MT2',0.04995],['MT3',0.05020],['MT4',0.05194],['MT5',0.05263],['1:20',0.05],['1:10',0.1],['7:24',7/24],['1:50',0.02]],
 8:[['Ø 10',10],['Ø 12',12],['Ø 16',16],['Ø 20',20],['Ø 25',25],['Ø 30',30],['Ø 40',40]]};
// Fields: [key, kind, default, decimals, step, group, shown]. Kinds: n number, o optional number, s choice
// (options in place 4). Group z (green, along the part), x (orange, diameter or depth) or c (the cut). Shown:
// D only with X in diameters, P only with X as depth. A default [a, b] is a for diameters, b for depth.
const LTF={0:[['feed','n',0.1,3,0.01,'c'],['dir','s',0,['left','right'],0,'c'],['zstop','o','',3,1,'z']],
 2:[['speed','n',0.5,2,0.1,'c'],['dir','s',0,['left','right'],0,'c'],['zstop','o','',3,1,'z']],
 4:[['za','n',0,3,1,'z'],['len','n',30,3,1,'z'],['dstock','n',25,3,0.5,'x','D'],['dfin','n',20,3,0.5,'x','D'],['depth','n',2,3,0.1,'x','P'],
   ['side','s',0,['ext','int'],0,'c'],['passes','n',8,0,1,'c'],['feed','n',0.1,3,0.01,'c']],
 5:[['za','n',0,3,0.1,'z'],['zdep','n',1,3,0.1,'z'],['dstock','n',30,3,1,'x','D'],['dfinf','n',0,3,1,'x','D'],['xin','n',15,3,1,'x','P'],
   ['passes','n',4,0,1,'c'],['feed','n',0.08,3,0.01,'c']],
 3:[['len','n',30,3,1,'z'],['dbig','n',40,3,0.5,'x'],['dsmall','n',30,3,0.5,'x'],['side','s',0,['ext','int'],0,'c'],['dir','s',0,['left','right'],0,'c'],['feed','n',0.08,3,0.01,'c']],
 7:[['za','n',-2,3,1,'z'],['len','n',20,3,1,'z'],['dnom','n',10,3,0.1,'x','D'],['tdep','n',0.92,3,0.01,'x'],
   ['pitch','n',1.5,3,0.05,'c'],['side','s',0,['ext','int'],0,'c'],['hand','s',0,['rh','lh'],0,'c'],['starts','n',1,0,1,'c'],['passes','n',10,0,1,'c']],
 6:[['dstock','n',25,3,0.5,'x','D'],['dfinp','n',0,3,0.5,'x','D'],['depth','n',12.5,3,0.5,'x','P'],['side','s',0,['ext','int'],0,'c'],['passes','n',1,0,1,'c'],['feed','n',0.04,3,0.01,'c']],
 8:[['za','n',0,3,0.5,'z'],['rz','n',12,3,0.5,'z'],['dstock','n',24,3,0.5,'x','D'],['rx','n',12,3,0.5,'x'],['shape','s',0,['convex','concave'],0,'c'],['passes','n',12,0,1,'c'],['feed','n',0.07,3,0.01,'c']]};
let ltOp=-1,ltPre={},ltV={},ltHelp=false;try{ltV=JSON.parse(localStorage.getItem('h4lathe3')||'{}')}catch(e){}
if(ltV.xdia===undefined)ltV.xdia=1;
function ltFill(){for(const m in LTF){ltV[m]=ltV[m]||{};for(const f of LTF[m])if(ltV[m][f[0]]===undefined)ltV[m][f[0]]=f[2]}}
ltFill();
const ltT=()=>LTT[look.lang=='el'?'el':'en'];
const ltSave=()=>{try{localStorage.setItem('h4lathe3',JSON.stringify(ltV))}catch(e){}};
const ltCur=()=>ltOp<0?(st.mode in LTF?st.mode:4):ltOp;
const ltShown=f=>!f[6]||(f[6]=='D')==!!ltV.xdia;
const ltFmt=(v,d)=>v===''||v==null?'':(+v).toFixed(d);
function ltLang(){const L=ltT();document.documentElement.lang=look.lang=='el'?'el':'en';
 for(const [t,n] of Object.entries(L.tabs)){const b=document.querySelector('#tabs button[data-t="'+t+'"] span');if(b)b.textContent=n}
 $('ltOps').innerHTML=LTOPS.map(([m,ic])=>`<button type="button" class="ltop" data-m="${m}"><svg viewBox="0 0 28 22" aria-hidden="true">${ic}</svg><span>${L.m[m]}</span></button>`).join('');
 $('ltRpmL').textContent=L.rpm;$('ltRpmU').textContent=L.rpmu;$('ltAngL').textContent=L.ang;$('ltStartT').textContent=L.start;
 const cols=['#8b95a6','#ff7a2e',AXC.Z,'var(--fg)'];$('ltLeg').innerHTML=L.legend.map((t,i)=>`<span><i style="background:${cols[i]}"></i>${t}</span>`).join('');
 $('ltXb').style.setProperty('--c',AXC.X);$('ltZb').style.setProperty('--c',AXC.Z);ltLast={};ltForm();if(st.lathe)ltShow()}
$('ltOps').addEventListener('click',e=>{const b=e.target.closest('.ltop');if(!b||st.on)return;ltOp=+b.dataset.m;ltForm();if(st.lathe)ltShow()});
function ltLabel(f,m){const L=ltT();if(f[0]=='dnom'&&ltV[m].side==1)return L.f.dbore;return L.f[f[0]]}
// The form is built once per mode, language or X kind; a change of a value only rewrites that value.
function ltForm(){const L=ltT(),m=ltCur(),v=ltV[m],F=LTF[m];if(!F)return;
 const zero=`<div class="ltzero"><b>${L.step1}</b><button type="button" data-z="z0">${L.z0}</button><button type="button" data-z="x0">${L.x0}</button><button type="button" data-z="dia">${L.dia}</button><button type="button" class="lthelpb" data-h="1">${L.help}</button>
  <span class="ltxin">${L.xin}</span><div class="seg" data-xk="1"><button type="button" data-i="1" class="${ltV.xdia?'on':''}">${L.xdia}</button><button type="button" data-i="0" class="${ltV.xdia?'':'on'}">${L.xdep}</button></div></div>
  <div class="lthelp"${ltHelp?'':' hidden'}>${L.helpText}</div>`;
 const fld=f=>f[1]=='s'
  ?`<div class="f"><label>${L.f[f[0]]}</label><div class="seg" data-k="${f[0]}">${f[3].map((o,i)=>`<button type="button" data-i="${i}" class="${v[f[0]]===i?'on':''}">${L.f[o]}</button>`).join('')}</div></div>`
  :`<div class="f"><label data-l="${f[0]}">${ltLabel(f,m)}</label><div class="step"><button type="button" data-k="${f[0]}" data-d="-1">−</button><button type="button" class="numv" data-k="${f[0]}">${ltFmt(v[f[0]],f[3])}</button><button type="button" data-k="${f[0]}" data-d="1">+</button></div></div>`;
 const grp=(g,title,cls)=>{const fs=F.filter(f=>f[5]==g&&ltShown(f));return fs.length?`<div class="ltg ${cls}"><h4>${title}</h4><div class="ltgf">${fs.map(fld).join('')}</div></div>`:''};
 const pre=LTPRE[m]?`<div class="f wide"><label for="ltPre">${L.f.preset}</label><select id="ltPre" class="sel"><option value="">—</option>${LTPRE[m].map((p,i)=>`<option value="${i}"${ltPre[m]===String(i)?' selected':''}>${p[0]}</option>`).join('')}</select></div>`:'';
 $('ltForm').innerHTML=`${zero}<h3>${L.step2}${L.t[m]}</h3>${pre}<div class="ltgroups">${grp('z',L.zx,'ltgz')}${grp('x',ltV.xdia?L.xx:L.xxd,'ltgx')}</div>${grp('c',L.cx,'ltgc')}<div class="ltinfo" id="ltInfo"></div>`;
 $('ltForm').querySelectorAll('.ltgz').forEach(e=>e.style.setProperty('--c',AXC.Z));$('ltForm').querySelectorAll('.ltgx').forEach(e=>e.style.setProperty('--c',AXC.X));
 ltLast={};ltInfo();ltDraw()}
function ltSet(m,k,val){const f=LTF[m].find(f=>f[0]==k);ltV[m][k]=val;ltPre[m]='';ltSave();
 const b=document.querySelector('#ltForm .numv[data-k="'+k+'"]');if(b)b.textContent=ltFmt(val,f[3]);
 const p=$('ltPre');if(p)p.value='';ltInfo();ltDraw()}
$('ltForm').addEventListener('click',async e=>{const b=e.target.closest('button');if(!b)return;const L=ltT(),m=ltCur(),v=ltV[m];
 if(b.dataset.h){ltHelp=!ltHelp;const h=$('ltForm').querySelector('.lthelp');if(h)h.hidden=!ltHelp;return}
 if(b.dataset.z){if(st.on){msg(L.busy);return}
  if(b.dataset.z=='dia'){ltPad(L.diaAsk,ltV.lastDia||'',false,async d=>{if(!(d>0))return;ltV.lastDia=d;try{await post('/lathe?do=dia&d='+d);ltV.xdia=1;ltSave();ltForm();msg(L.zeroDone)}catch(err){msg(err.message)}});return}
  try{await post('/lathe?do='+b.dataset.z);if(b.dataset.z=='x0'&&ltV.xdia){ltV.xdia=0;ltSave();ltForm()}msg(L.zeroDone)}catch(err){msg(err.message)}return}
 if(st.on)return;
 const sg=b.closest('.seg');if(sg){const i=+b.dataset.i;if(sg.dataset.xk){if(ltV.xdia!=i){ltV.xdia=i;ltSave();ltForm()}return}
  v[sg.dataset.k]=i;ltPre[m]='';ltSave();if(sg.dataset.k=='side'&&m==7){ltForm();return}
  sg.querySelectorAll('button').forEach(x=>x.classList.toggle('on',x==b));const p=$('ltPre');if(p)p.value='';ltInfo();ltDraw();return}
 const k=b.dataset.k;if(!k)return;const f=LTF[m].find(f=>f[0]==k);
 if(b.classList.contains('numv')){ltPad(ltLabel(f,m),v[k],f[1]=='o',n=>{if(n===''&&f[1]!='o')return;if(f[3]==0)n=Math.max(1,Math.round(n));ltSet(m,k,n)});return}
 let n=(v[k]===''?(k=='zstop'?+(st.z||0).toFixed(1):0):+v[k])+f[4]*b.dataset.d;n=+n.toFixed(5);if(f[3]==0||k=='feed'||k=='pitch'||k=='speed')n=Math.max(f[3]==0?1:f[4],n);ltSet(m,k,n)});
$('ltForm').addEventListener('change',e=>{const m=ltCur(),v=ltV[m],t=e.target;if(t.id!='ltPre')return;ltPre[m]=t.value;
 if(t.value!==''){const p=LTPRE[m][+t.value];
  if(m==7){const D=p[1],P=p[2],int=v.side==1;v.pitch=+P.toFixed(4);v.dnom=+(int?D-1.0825*P:D).toFixed(3);v.tdep=+((int?0.5413:0.6134)*P).toFixed(3)}
  if(m==3)v.dsmall=+(v.dbig-p[1]*v.len).toFixed(3);
  if(m==8){v.rz=v.rx=p[1]/2;v.dstock=p[1]}}
 ltSave();const keep=t.value;ltForm();if($('ltPre'))$('ltPre').value=keep;ltPre[m]=keep;if(st.lathe)ltShow()});
// A numeric keypad of the page itself: the tablet's own keyboard would cover the form.
function ltPad(title,val,optional,done){const L=ltT();let s=val===''||val==null?'':String(+val);
 const o=document.createElement('div');o.className='ltpad';
 o.innerHTML=`<div class="ltpadb"><div class="ltpadt">${title}</div><div class="ltpadv" id="ltPadV"></div><div class="ltpadk">${['7','8','9','4','5','6','1','2','3','±','0','.'].map(k=>`<button type="button" data-p="${k}">${k}</button>`).join('')}
  <button type="button" data-p="b">⌫</button>${optional?`<button type="button" data-p="e">${L.clear}</button>`:'<span></span>'}<button type="button" data-p="c">${L.cancel}</button><button type="button" class="ok" data-p="o">${L.ok}</button></div></div>`;
 const show=()=>{o.querySelector('#ltPadV').textContent=s===''?'—':s};let first=true;
 o.addEventListener('click',e=>{const b=e.target.closest('button');if(!b){if(e.target==o)o.remove();return}const k=b.dataset.p;
  if(k=='c'){o.remove();return}if(k=='o'){o.remove();if(s===''){if(optional)done('');return}const n=parseFloat(s);if(isFinite(n))done(n);return}
  if(k=='e'){o.remove();done('');return}
  if(first&&/[0-9.]/.test(k))s='';first=false;
  if(k=='b')s=s.slice(0,-1);else if(k=='±')s=s.startsWith('-')?s.slice(1):'-'+s;else if(k=='.'){if(!s.includes('.'))s+=(s===''||s=='-'?'0':'')+'.'}else if(s.replace(/[-.]/g,'').length<8)s+=k;show()});
 document.body.appendChild(o);show()}
// What the form becomes on the H4: work coordinates (the part's zero), as the H4 displays them. X grows towards the
// centre: with X0 on the centre a diameter D is X = -D/2; with X0 on the surface the depth d is X = +d outside.
function ltPlan(){const m=ltCur(),v=ltV[m],q={mode:m},lo=Math.min,hi=Math.max,dia=!!ltV.xdia,R=d=>-d/2,in_=v.side==1;
 const X0=dia?R(v.dstock!==undefined?v.dstock:v.dnom):0;
 if(m==0||m==2){const p=m==0?v.feed:v.speed;q.pitch=(v.dir?-1:1)*p;q.zl=q.zr='';if(v.zstop!==''){if(v.dir)q.zr=v.zstop;else q.zl=v.zstop}}
 if(m==4){const xb=dia?R(v.dfin):(in_?-v.depth:v.depth);q.pitch=v.feed;q.passes=v.passes;q.aux=in_?0:1;q.zr=v.za;q.zl=v.za+v.len;q.xr=lo(X0,xb);q.xl=hi(X0,xb);q.start=[v.za,X0];q.xb=xb}
 if(m==5){const xb=dia?R(v.dfinf):v.xin;q.pitch=(xb>=X0?1:-1)*v.feed;q.passes=v.passes;q.aux=1;q.zr=v.za;q.zl=v.za+v.zdep;q.xr=lo(X0,xb);q.xl=hi(X0,xb);q.start=[v.za,X0];q.xb=xb}
 if(m==3){q.pitch=(v.dir?-1:1)*v.feed;q.cone=Math.abs(v.dbig-v.dsmall)/v.len;q.aux=in_?0:1;q.xl=q.xr='';const z=st.z||0;if(v.dir){q.zr=z-v.len;q.zl=''}else{q.zl=z+v.len;q.zr=''}}
 if(m==7){const xa=dia?R(v.dnom):0,xb=xa+(in_?-v.tdep:v.tdep);q.pitch=(v.hand?-1:1)*v.pitch;q.starts=v.starts;q.passes=v.passes;q.aux=in_?0:1;q.zr=v.za;q.zl=v.za+v.len;q.xr=lo(xa,xb);q.xl=hi(xa,xb);q.start=[v.hand?q.zl:q.zr,xa];q.xb=xb}
 if(m==6){const xb=dia?R(v.dfinp):(in_?-v.depth:v.depth);q.pitch=(xb>=X0?1:-1)*v.feed;q.passes=v.passes;q.xr=lo(X0,xb);q.xl=hi(X0,xb);q.start=[null,X0];q.xb=xb}
 if(m==8){q.pitch=v.feed;q.passes=v.passes;q.aux=v.shape?0:1;q.zr=v.za;q.zl=v.za+v.rz;q.xr=X0;q.xl=X0+v.rx;q.start=[v.za,X0]}
 return q}
function ltCheck(){const L=ltT(),m=ltCur(),v=ltV[m],E=L.err,F=LTF[m].filter(ltShown);
 for(const f of F)if(f[1]=='n'&&!isFinite(+v[f[0]]))return E.val;
 for(const f of F)if(f[1]=='n'&&/^d/.test(f[0])&&v[f[0]]<0)return E.dia;
 for(const k of ['len','zdep','rz','rx','tdep','depth','xin'])if(F.some(f=>f[0]==k)&&!(v[k]>0))return E.len;
 if(m==3&&!(v.dbig>v.dsmall))return E.cone;
 if(ltV.xdia&&(m==4||m==6)){const fin=m==4?v.dfin:v.dfinp;if(fin==v.dstock)return E.same;
  if((fin>v.dstock)!=(v.side==1))return E.side.replace('{w}',v.side==1?E.larger:E.smaller)}
 if(ltV.xdia&&m==5&&v.dfinf==v.dstock)return E.same;
 return ''}
let ltLast={};const ltPut=(id,html)=>{if(ltLast[id]!==html){ltLast[id]=html;const e=$(id);if(e)e.innerHTML=html}};
function ltInfo(){if(!$('ltInfo')||!st.lathe)return;const L=ltT(),m=ltCur(),v=ltV[m],q=ltPlan(),out=[],err=ltCheck(),f3=x=>(+x).toFixed(3);
 if(err)out.push('<b class="bad">'+err+'</b>');
 else{if(q.start&&q.start[0]!=null)out.push(L.goTo.replace('{z}',f3(q.start[0])).replace('{x}',f3(q.start[1])+(ltV.xdia?' (Ø '+f3(-2*q.start[1])+')':'')));
  if(m==6)out.push(L.cutZ.replace('{z}',f3(st.z)));
  if(m==0||m==2||m==3)out.push(L.fromTool+(m==3?' '+L.coneStop.replace('{l}',f3(v.len))+' k = '+(+q.cone).toFixed(5):''));
  if(ltV.xdia&&LTF[m].some(f=>f[5]=='x'&&f[6]=='D'))out.push(L.x0note)}
 const p=Math.abs(q.pitch||0),dx=q.xb!==undefined&&q.start?Math.abs(q.xb-q.start[1]):0;let mm=0;
 if(m==4)mm=v.len*v.passes;if(m==7)mm=v.len*v.passes*v.starts;if(m==5)mm=dx*v.passes;if(m==6)mm=dx;if(m==3)mm=v.len;if(m==8)mm=(v.rz+v.rx)*1.2*v.passes;
 if(mm&&p&&m!=2&&!err){if(st.rpm>2){const t=mm/(p*st.rpm)*1.15;
   out.push('≈ '+(t<1?Math.max(1,Math.round(t*60))+(look.lang=='el'?' δευτ.':' s'):t.toFixed(1)+(look.lang=='el'?' λεπτά':' min'))+' '+L.estimate+' '+st.rpm+' '+L.rpmu)}else out.push(L.stopped)}
 ltPut('ltInfo',out.join('<br>'))}
function ltShow(){const L=ltT(),l=st.lathe;if(!l)return;const u=st.inch?'in':'mm';
 document.querySelectorAll('.ltop').forEach(b=>{b.classList.toggle('on',+b.dataset.m==st.mode);b.classList.toggle('sel',+b.dataset.m==ltCur()&&ltCur()!=st.mode);b.disabled=!!st.on});
 $('ltXv').textContent=st.x.toFixed(3);$('ltXu').textContent=u;$('ltZv').textContent=st.z.toFixed(3);$('ltZu').textContent=u;
 $('ltRpm').textContent=st.rpm;
 // the angle only means something while the spindle stands or creeps: no number racing past, no animation
 ltPut('ltAng',Math.abs(st.rpm)>10?'<span class="ltturn">'+L.turning+'</span>':(+l.ang).toFixed(1)+'<small>°</small>');
 const f=v=>v==null?L.none:(+v).toFixed(3),m=st.mode;
 const now=[L.m[m]||m,(m==7?'P ':'')+Math.abs(l.pitch).toFixed(3)+(l.pitch<0?' ←':''),[4,5,6,7,8].includes(m)?l.passes+' '+L.passes:'',m==7&&l.starts>1?l.starts+'×':'',m==3?'k '+(+l.cone).toFixed(5):'',
  'Z '+f(l.zr)+'…'+f(l.zl),'X '+f(l.xr)+'…'+f(l.xl)].filter(Boolean).join(' · ');
 ltPut('ltNow',(l.measure?L.inch+' ':'')+(m==9?L.gcode:L.now+': '+now));
 const sb=$('ltStart');sb.classList.toggle('run',!!st.on);$('ltStartT').textContent=st.on?L.running:L.start;
 const pm=[4,5,6,7,8].includes(m);$('ltBar').style.width=st.on&&pm&&l.passes?Math.min(100,100*l.op/l.passes)+'%':'0';
 $('ltProg').textContent=(L.m[m]||'')+(st.on&&pm?' · '+L.pass+' '+Math.min(l.op+1,l.passes)+' / '+l.passes:(pm?' · '+l.passes+' '+L.passes:'')+' · '+(st.on?'':L.ready));
 ltInfo();ltDraw()}
// START: send the form (the H4 refuses while it is ON or moving), then press ON through the H4's key queue.
async function ltStartGo(){const L=ltT();if(st.on)return;if(st.lathe&&st.lathe.measure){msg(L.inch);return}
 const err=ltCheck();if(err){msg(err);return}
 const q=ltPlan(),p=new URLSearchParams();for(const [k,v] of Object.entries(q))if(k!='start'&&k!='xb')p.set(k,v===''?'':(typeof v=='number'?+v.toFixed(5):v));
 try{await post('/lathe?'+p.toString());await new Promise(r=>setTimeout(r,300));await post('/lathe?do=start')}catch(e){msg(e.message)}}
(function(){const b=$('ltStart');let t=null,t0=0;const end=()=>{clearInterval(t);t=null;$('ltFill').style.width='0'};
 const go=e=>{if(st.on||t)return;e.preventDefault();t0=performance.now();t=setInterval(()=>{const k=Math.min(1,(performance.now()-t0)/800);$('ltFill').style.width=k*100+'%';if(k>=1){end();ltStartGo()}},30)};
 b.addEventListener('pointerdown',go);['pointerup','pointerleave','pointercancel'].forEach(v=>b.addEventListener(v,end));
 b.addEventListener('keydown',e=>{if(e.key=='Enter'||e.key==' ')go(e)});b.addEventListener('keyup',end)})();
// Side view, fitted to Z0, the stops (or the form's plan) and the tool; X0 as the H4 has it, the outside of the
// part at the bottom so the tool comes from below as on the lathe.
function ltDraw(){const l=st.lathe;if(!l)return;const W=600,H=300,q=st.on?null:ltPlan(),src=st.on||!q?l:{zl:q.zl,zr:q.zr,xl:q.xl,xr:q.xr};
 const num=v=>v===''||v==null?null:+v,zl=num(src.zl),zr=num(src.zr),xl=num(src.xl),xr=num(src.xr),zs=[0,st.z],xs=[0,st.x];if(!ltV.xdia&&xl!=null)xs.push(xl+2);
 for(const v of [zl,zr])if(v!=null)zs.push(v);for(const v of [xl,xr])if(v!=null)xs.push(v);
 let z0=Math.min(...zs),z1=Math.max(...zs),x0=Math.min(...xs),x1=Math.max(...xs);
 const zr_=Math.max(10,z1-z0),xr_=Math.max(5,x1-x0);z0-=zr_*.15;z1+=zr_*.35;x0-=xr_*.25;x1+=xr_*.25;
 const S=Math.min((W-120)/(z1-z0),(H-40)/(x1-x0)),px=z=>W-30-(z-z0)*S,py=x=>H-20-(x-x0)*S,g=[];  // outside at the bottom: the tool comes from below
 const ax=ltV.xdia?py(0):12,chuck=px(z1);
 for(let x=20;x<W;x+=40)g.push(`<line x1="${x}" y1="10" x2="${x}" y2="${H-10}" stroke="var(--tileb)" stroke-width="1"/>`);
 const surf=xr!=null?xr:(ltV.xdia?Math.min(st.x,0):0),top=py(surf);
 g.push(`<rect x="${chuck}" y="${Math.min(top,ax)}" width="${Math.max(0,px(0)-chuck)}" height="${Math.abs(ax-top)}" fill="#8b95a6" opacity=".55"/>`);
 g.push(`<rect x="${chuck-40}" y="${Math.min(top,ax)-30}" width="40" height="${Math.abs(ax-top)+60}" rx="5" fill="var(--btn)" stroke="var(--tileb)"/>`);
 if(zl!=null&&zr!=null&&xl!=null&&xr!=null){const a=px(zl),b=px(zr),c=py(xl),d=py(xr);
  g.push(`<rect x="${Math.min(a,b)}" y="${Math.min(c,d)}" width="${Math.abs(b-a)}" height="${Math.abs(d-c)}" fill="#ff7a2e" opacity=".55"/>`)}
 if(ltV.xdia)g.push(`<line x1="10" y1="${ax}" x2="${W-10}" y2="${ax}" stroke="var(--dim)" stroke-dasharray="10 4 2 4"/>`);
 const vl=(z,t)=>g.push(`<line x1="${px(z)}" y1="10" x2="${px(z)}" y2="${H-10}" stroke="${AXC.Z}" stroke-dasharray="4 4"/><text x="${px(z)+4}" y="24" fill="${AXC.Z}" font-size="13" font-weight="700">${t}</text>`);
 vl(0,'Z0');if(zl!=null)vl(zl,'◂');if(zr!=null&&zr!=0)vl(zr,'▸');
 const hl=x=>g.push(`<line x1="10" y1="${py(x)}" x2="${W-10}" y2="${py(x)}" stroke="${AXC.X}" stroke-dasharray="4 4" opacity=".8"/>`);
 if(xl!=null)hl(xl);if(xr!=null)hl(xr);
 const tx=px(st.z),ty=py(st.x);g.push(`<g transform="translate(${tx},${ty})"><path d="M0 0 L-8 26 L18 26 Z" fill="var(--fg)"/><rect x="-6" y="26" width="26" height="30" fill="var(--dim)"/></g>`);
 ltPut('ltSvg',g.join(''))}
window.ltReady=true;ltLang();
if(DEMO)$('demoTag').hidden=false;
scApply();
{let t0=location.hash.slice(1);if(!TN.includes(t0))try{t0=localStorage.getItem('h4tab')}catch(e){}t0=TN.includes(t0)?t0:'control';
 tab(t0,true);try{history.replaceState({t:t0},'','#'+t0)}catch(e){}}
setList();listFiles();loadCfg();poll();
</script></body></html>
)rawpage";
