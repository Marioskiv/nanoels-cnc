// Touch-off guard of the Lathe page: START waits for the zeros after the first use, an H4 restart, a tool change and
// a new part; a zero changed on the H4 (keys) asks for a confirmation. Runs the page's demo in a real browser.
// Usage: node guard.js <page.html>
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:1280,height:800}});
 const errs=[];p.on('pageerror',e=>errs.push(String(e)));
 await p.goto('file://'+require('path').resolve(process.argv[2])+'?demo');await p.waitForTimeout(700);
 // app 1.35: the operations on the part need the Part (a large one: it does not limit these checks)
 await p.evaluate(()=>{ltPart={d:100,len:250,bore:0,boot:st.boot,z0:0-partOff(),ro:Array(2501).fill(50),ri:Array(2501).fill(0),thr:[],cut:0};partSave()});
 await p.evaluate(()=>{const t=[...document.querySelectorAll('[data-t]')].find(b=>/lathe/i.test(b.dataset.t));t.click()});await p.waitForTimeout(400);
 const fails=[];let n=0;const ok=(c,w)=>{n++;if(!c)fails.push(w)};
 const S=()=>p.evaluate(()=>({g:gdState(),note4:gdNote(4,false),note6:gdNote(6,false),note0:gdNote(0,false),info:document.getElementById('ltInfo').textContent,
   needZ:document.querySelector('#ltForm [data-z="z0"]').classList.contains('need'),needX:document.querySelector('#ltForm [data-z="dia"]').classList.contains('need')}));
 const click=async sel=>{await p.click(sel);await p.waitForTimeout(100)};
 const pad=async keys=>{for(const k of keys)await p.click(`.ltpad button[data-p="${k}"]`);await p.click('.ltpad button[data-p="o"]')};
 await p.evaluate(()=>{ltOp=4;Object.assign(ltV[4],{side:0,dstock:25,dfin:20,za:0,len:30,passes:4,feed:0.1});ltForm()});await p.waitForTimeout(300);
 let s=await S();
 ok(s.g.z=='need'&&s.g.x=='need'&&s.needZ&&s.needX,'first use: both needed '+JSON.stringify(s));
 ok(/ΕΚΚΙΝΗΣΗ|START/.test(s.note4)&&s.note0==='','turning guarded, feed not');
 ok(/Before START|Πριν την ΕΚΚΙΝΗΣΗ/.test(s.info),'note shown under the form');
 // START refused, and nothing sent
 const sent0=await p.evaluate(()=>st.on);await p.evaluate(()=>ltStartGo());await p.waitForTimeout(300);
 ok(!(await p.evaluate(()=>st.on))&&!sent0,'START refused while zeros are missing');
 // touch Z0, then Ø -> X0 (the page's keypad)
 await click('#ltForm [data-z="z0"]');await p.waitForTimeout(1500);s=await S();ok(s.g.z==''&&s.g.x=='need'&&!s.needZ&&s.needX,'after Z0: only X '+JSON.stringify(s.g));
 ok(s.note6!=='','parting still needs X');
 await click('#ltForm [data-z="dia"]');await pad('25');await p.waitForTimeout(1500);s=await S();
 ok(s.g.z==''&&s.g.x==''&&s.note4===''&&!/Before START|Πριν την ΕΚΚΙΝΗΣΗ/.test(s.info),'after both: START allowed '+JSON.stringify(s.g));
 // a zero changed on the H4 (as from its keys: not through the page's buttons) asks for a confirmation
 // (the demo's zero is moved by 1 mm against what the page remembers: the same as a Z0 set 1 mm away on the H4)
 await p.evaluate(()=>{ltGd.z+=1;gdSave()});await p.waitForTimeout(700);s=await S();
 ok(s.g.z=='changed'&&/H4/.test(s.note4),'zero changed on the H4 -> confirmation '+JSON.stringify(s.g));
 ok(await p.$('#ltInfo [data-gd="ok"]')!==null,'confirm button shown');
 await click('#ltInfo [data-gd="ok"]');await p.waitForTimeout(400);s=await S();ok(s.g.z==''&&s.note4==='','confirmed');
 // tool changed: both again
 await click('#ltForm [data-gd="tool"]');await p.waitForTimeout(400);s=await S();ok(s.g.z=='need'&&s.g.x=='need'&&/tool|κοπτικό/.test(s.note4),'tool changed '+s.note4);
 await click('#ltForm [data-z="z0"]');await p.waitForTimeout(1500);await click('#ltForm [data-z="dia"]');await pad('25');await p.waitForTimeout(1500);
 s=await S();ok(s.note4==='','zeros again after the tool change');
 // H4 restart (another boot number): the H4 kept the zeros (app 1.33): asks to confirm, START held until then
 await p.evaluate(()=>{ltGd.boot=0;gdSave()});await p.waitForTimeout(700);s=await S();
 ok(s.g.z=='changed'&&s.g.x=='changed'&&/restarted|επανεκκίνησε/.test(s.note4)&&/kept|κράτησε/.test(s.note4),'restart: zeros kept, confirm '+s.note4);
 await p.evaluate(()=>ltStartGo());await p.waitForTimeout(300);ok(!(await p.evaluate(()=>st.on||!!ltJob)),'START held after a restart');
 await click('#ltInfo [data-gd="ok"]');await p.waitForTimeout(400);s=await S();ok(s.g.z==''&&s.g.x==''&&s.note4==='','restart confirmed: zeros kept');
 // or touched again (one axis touched clears that one)
 await p.evaluate(()=>{ltGd.boot=0;gdSave()});await p.waitForTimeout(700);
 await click('#ltForm [data-z="z0"]');await p.waitForTimeout(1500);s=await S();ok(s.g.z==''&&s.g.x=='changed','restart: Z touched again, X still to confirm');
 await click('#ltForm [data-z="dia"]');await pad('25');await p.waitForTimeout(1500);s=await S();ok(s.note4==='','restart: both touched again');
 // new part: Z0 on its face only; Z0 then moves the part's face there
 await p.evaluate(()=>{partNew(25,60,0);ltForm()});await p.waitForTimeout(400);s=await S();ok(s.g.z=='need'&&s.g.x=='','new part: only Z0 '+JSON.stringify(s.g));
 const before=await p.evaluate(()=>ltPart.z0);await click('#ltForm [data-z="z0"]');await p.waitForTimeout(1500);
 s=await S();const face=await p.evaluate(()=>partZ(0));ok(s.g.z==''&&Math.abs(face)<1e-6,'Z0 on the new part: face at Z0 '+before+' '+face);
 // START goes through now (demo)
 await p.evaluate(()=>ltStartGo());await p.waitForTimeout(2500);ok(await p.evaluate(()=>!!st.on||!!ltJob),'START allowed with the zeros set');
 await b.close();
 const all=fails.concat(errs.map(e=>'page error: '+e));all.forEach(f=>console.log('FAIL',f));
 console.log('guard: '+n+' checks, '+all.length+' failures');process.exit(all.length?1:0)})();
