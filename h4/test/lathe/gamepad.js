// The gamepad page (app 1.36): what each button does, alone and with the safety button held. A map without a STOP
// or a safety button is refused; nothing moves and Home All / touch-off / Resume do nothing without the safety
// button. A fake gamepad (navigator.getGamepads) drives the page's own loop. Usage: node gamepad.js <page.html>
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const fails=[];let n=0;const ok=(c,w)=>{n++;if(!c)fails.push(w)};const errs=[];
 const p=await b.newPage({viewport:{width:1024,height:600}});p.on('pageerror',e=>errs.push(String(e)));
 await p.addInitScript(()=>{localStorage.setItem('h4look',JSON.stringify({lang:'en'}));
  // a fake standard gamepad the test sets button by button; requests are recorded
  window.FP={buttons:Array.from({length:17},()=>({pressed:false,value:0})),axes:[0,0,0,0],connected:true,mapping:'standard',id:'Fake pad'};
  navigator.getGamepads=()=>[window.FP];window.REQ=[];const f=window.fetch;window.fetch=(u,o)=>{window.REQ.push(String(u));return f(u,o)}});
 await p.goto('file://'+require('path').resolve(process.argv[2])+'?demo');await p.waitForTimeout(700);
 await p.evaluate(()=>{const o=window.post;window.post=(u,b)=>{window.REQ.push(String(u)+(b?'?'+b:''));return o(u,b)};const f=window.fetch;window.fetch=(u,x)=>{window.REQ.push(String(u));return f(u,x)};tab('settings');setShow('pad')});await p.waitForTimeout(200);
 // the default map: the LinuxCNC gamepad.hal layout
 let r=await p.evaluate(()=>[gp.map.l1[0],gp.map.options[0],gp.map.share[1],gp.map.down.join('|'),gpCheck(gp.map),document.querySelectorAll('.spage[data-sp=pad] select').length]);
 ok(r[0]=='enable'&&r[1]=='stop'&&r[2]=='home'&&r[3]=='fdn|z0'&&r[4]===''&&r[5]==36,'default map '+r);
 // refused: no STOP, no safety button, a second action on the safety button
 const set=(k,i,v)=>p.evaluate(([k,i,v])=>{const s=document.querySelector(`.spage[data-sp=pad] select[data-gb="${k}"][data-gi="${i}"]`);s.value=v;s.dispatchEvent(new Event('change',{bubbles:true}));return [JSON.stringify(gp.map[k]),$('gpBad').textContent]},[k,i,v]);
 r=await set('options',0,'');ok(r[0]=='["stop","stop"]'&&/STOP/.test(r[1]),'removing the only STOP refused '+r);
 r=await set('l1',0,'fast');ok(r[0]=='["enable",""]'&&/safety/.test(r[1]),'removing the safety button refused '+r);
 ok(await p.evaluate(()=>document.querySelector('.spage[data-sp=pad] select[data-gb="l1"][data-gi="1"]').disabled),'the safety button has no second action');
 // a change that is fine is kept (and survives a reload)
 r=await set('cross',1,'resume');ok(r[0]=='["","resume"]'&&r[1]==='','cross + safety = resume kept '+r);
 ok(await p.evaluate(()=>JSON.parse(localStorage.getItem('h4gamepad')).map.cross[1]=='resume'),'saved');
 // drive the fake pad
 const press=async(i,on)=>{await p.evaluate(([i,on])=>{FP.buttons[i]={pressed:on,value:on?1:0}},[i,on]);await p.waitForTimeout(80)};
 const reqs=()=>p.evaluate(()=>{const r=REQ.slice();REQ.length=0;return r.join(' ')});
 await reqs();
 await press(8,true);await press(8,false);ok(!/home/.test(await reqs()),'Share alone: no Home All');
 await press(0,true);await press(0,false);ok(!/resume/.test(await reqs()),'Cross alone: no Resume');
 await press(4,true);await press(8,true);await press(8,false);await press(0,true);await press(0,false);await press(13,true);await press(13,false);
 r=await reqs();ok(/do=home/.test(r)&&/do=resume/.test(r)&&/G10/.test(r),'with L1: Home All, Resume, touch-off Z0: '+r);
 await press(4,false);await press(13,true);await press(13,false);r=await reqs();ok(/code=146/.test(r)&&!/G10/.test(r),'D-pad down alone: feed -10 %: '+r);
 await press(9,true);await press(9,false);ok(/do=stop/.test(await reqs()),'Options: STOP');
 // sticks: nothing moves without the safety button; with it, the left stick moves Z
 await p.evaluate(()=>{FP.axes=[1,0,0,0]});await p.waitForTimeout(250);ok(!/\/jog\?x=0\.0&z=[1-9]/.test(await reqs()),'stick without safety: no move');
 await press(4,true);await p.waitForTimeout(250);r=await reqs();ok(/\/jog\?x=0\.0&z=400\.0/.test(r),'stick with safety: Z moves '+r.slice(0,120));
 // the right stick set to Z, the left one to nothing
 await p.evaluate(()=>{$('gpSL').value='';$('gpSL').dispatchEvent(new Event('change'));$('gpSR').value='z';$('gpSR').dispatchEvent(new Event('change'));FP.axes=[1,0,0,1]});
 await p.waitForTimeout(300);r=await reqs();ok(/\/jog\?x=0\.0&z=400\.0/.test(r),'right stick moves Z when set so');
 await press(4,false);await p.evaluate(()=>{FP.axes=[0,0,0,0]});await p.waitForTimeout(400);ok(/\/jog\?x=0\.0&z=0\.0&c=0\.0/.test(await reqs()),'release: stop sent');
 // Defaults brings the layout back
 await p.evaluate(()=>gpDefaults());r=await p.evaluate(()=>[gp.map.cross,gp.sl,gp.sr]);ok(r[0]===undefined,'defaults: Cross does nothing again '+r);ok(r[1]=='z'&&r[2]=='x','sticks back');
 await b.close();
 const all=fails.concat(errs.map(e=>'page error: '+e));all.forEach(f=>console.log('FAIL',f));
 console.log('gamepad: '+n+' checks, '+all.length+' failures');process.exit(all.length?1:0)})();
