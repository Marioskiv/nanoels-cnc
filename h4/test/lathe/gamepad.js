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
 // dead zones (app 1.49): a worn stick that rests at 40 % moves Z with the default 35 %; with 45 % it does not.
 // The indicator shows where the stick is, the dead zone, and green when it would move.
 await p.evaluate(()=>{$('gpSL').value='z';$('gpSL').dispatchEvent(new Event('change'));$('gpSR').value='x';$('gpSR').dispatchEvent(new Event('change'))});
 r=await p.evaluate(()=>[$('gpDL').value,$('gpDR').value,$('gpDT').value]);ok(r.join()=='35,45,5','dead zone defaults 35 / 45 / 5 % '+r);
 await p.evaluate(()=>{FP.axes=[0.4,0,0,0]});await press(4,true);await p.waitForTimeout(250);r=await reqs();
 ok(/\/jog\?x=0\.0&z=[1-9]/.test(r),'stick at 40 %, dead zone 35 %: Z moves '+r.slice(0,80));
 r=await p.evaluate(()=>{const a=$('gpAx').children[0];return [a.classList.contains('on'),a.querySelector('.v').textContent,a.querySelector('.m').style.left,a.querySelector('.dz').style.width]});
 ok(r[0]&&r[1]=='40%'&&r[2]=='70%'&&r[3]=='35%','indicator: left stick 40 %, marker at 70 %, dead zone 35 % wide, green '+r);
 await p.evaluate(()=>{$('gpDL').value='45';$('gpDL').dispatchEvent(new Event('change'))});await reqs();await p.waitForTimeout(300);r=await reqs();
 ok(!/\/jog\?x=0\.0&z=[1-9]/.test(r),'dead zone 45 %: the stick at 40 % does not move Z '+r.slice(0,80));
 r=await p.evaluate(()=>[$('gpAx').children[0].classList.contains('on'),JSON.parse(localStorage.getItem('h4gamepad')).dl]);ok(!r[0]&&r[1]==45,'indicator grey, 45 saved '+r);
 r=await p.evaluate(()=>{const e=$('gpDL');e.value='150';e.dispatchEvent(new Event('change'));const a=[gp.dl,e.value];e.value='0';e.dispatchEvent(new Event('change'));a.push(gp.dl);e.value='abc';e.dispatchEvent(new Event('change'));a.push(gp.dl);return a});
 ok(r[0]==90&&r[1]=='90'&&r[2]===0&&r[3]==35,'dead zone kept within 0 .. 90 %, empty = default '+r);
 await p.evaluate(()=>{$('gpDL').value='45';$('gpDL').dispatchEvent(new Event('change'));FP.axes=[0,0,0,0];FP.buttons[6]={pressed:true,value:0.5}});await p.waitForTimeout(100);
 r=await p.evaluate(()=>{const a=$('gpAx').children[2];return [a.querySelector('.v').textContent,a.classList.contains('on')]});ok(r[0]=='50%'&&r[1],'indicator: L2 half pressed '+r);
 await p.evaluate(()=>{FP.buttons[6]={pressed:false,value:0}});await press(4,false);await p.waitForTimeout(400);await reqs();
 // the app's own reading (H4 CNC app): h4AppPadAxes, and nothing when the pad is gone
 r=await p.evaluate(()=>{h4AppPadAxes(-0.2,0.5,0,1);const c=$('gpAx').children;const a=[c[0].querySelector('.v').textContent,c[1].classList.contains('on'),c[3].querySelector('.v').textContent];h4AppPadAxes();a.push(c[0].querySelector('.v').textContent);return a});
 ok(r.join()=='-20%,true,100%,-','app reading shown, "-" without a pad '+r);
 // Defaults brings the layout back
 await p.evaluate(()=>gpDefaults());r=await p.evaluate(()=>[gp.map.cross,gp.sl,gp.sr]);ok(r[0]===undefined,'defaults: Cross does nothing again '+r);ok(r[1]=='z'&&r[2]=='x','sticks back');
 await b.close();
 const all=fails.concat(errs.map(e=>'page error: '+e));all.forEach(f=>console.log('FAIL',f));
 console.log('gamepad: '+n+' checks, '+all.length+' failures');process.exit(all.length?1:0)})();
