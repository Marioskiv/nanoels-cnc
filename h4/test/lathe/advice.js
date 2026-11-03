// Spindle speed / feed starting point of the Lathe page: rpm from Vc and the largest Ø (at most 1000), depth per pass
// and feed checks, none for threads. Runs the page's demo in a real browser. Usage: node advice.js <page.html>
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:1280,height:800}});
 const errs=[];p.on('pageerror',e=>errs.push(String(e)));
 await p.addInitScript(()=>localStorage.setItem('h4look',JSON.stringify({lang:'en'})));
 await p.goto('file://'+require('path').resolve(process.argv[2])+'?demo');await p.waitForTimeout(700);
 // app 1.35: the operations on the part need the Part (a large one: it does not limit these checks)
 await p.evaluate(()=>{ltPart={d:100,len:250,bore:0,boot:st.boot,z0:0-partOff(),ro:Array(2501).fill(50),ri:Array(2501).fill(0),thr:[],cut:0};partSave()});
 await p.evaluate(()=>{const t=[...document.querySelectorAll('[data-t]')].find(b=>/lathe/i.test(b.dataset.t));t.click()});await p.waitForTimeout(400);
 const fails=[];let n=0;const ok=(c,w)=>{n++;if(!c)fails.push(w)};
 const set=(m,v,extra)=>p.evaluate(([m,v,extra])=>{Object.assign(ltV,extra||{});ltOp=m;Object.assign(ltV[m],v);ltForm();return document.getElementById('ltInfo').textContent},[m,v,extra]);
 // steel, carbide, turning Ø50: 1000*120/(π*50) = 764 -> 760
 let t=await set(4,{side:0,dstock:50,dfin:46,passes:4,feed:0.1},{mat:1,tm:0});ok(/≈ 760 rpm/.test(t)&&/Vc 120/.test(t),'steel carbide Ø50: '+t.slice(0,300));
 // aluminium carbide Ø20: 3183 -> capped 1000
 t=await set(4,{side:0,dstock:20,dfin:18,passes:1,feed:0.1},{mat:0,tm:0});ok(/≈ 1000 rpm/.test(t)&&/1000 rpm\)/.test(t),'aluminium capped: '+t.slice(0,200));
 // steel HSS Ø40: 1000*30/(π*40)=238.7 -> 240
 t=await set(4,{side:0,dstock:40,dfin:38,passes:2,feed:0.1},{mat:1,tm:1});ok(/≈ 240 rpm/.test(t),'steel HSS Ø40');
 // bore: the final Ø counts (largest)
 t=await set(4,{side:1,dstock:20,dfin:30,passes:10,feed:0.1},{mat:1,tm:0});ok(/at Ø 30\.00/.test(t),'bore uses the final Ø');
 // too deep: steel 0.5 mm/pass -> 2 mm radial in 1 pass -> 4 passes, button sets them
 t=await set(4,{side:0,dstock:40,dfin:36,passes:1,feed:0.1},{mat:1,tm:0});ok(/4 passes/.test(t),'deep -> 4 passes: '+t);
 await p.evaluate(()=>document.querySelector('#ltInfo [data-advp]').click());await p.waitForTimeout(200);ok(await p.evaluate(()=>ltV[4].passes==4),'passes set to 4');
 ok(!/deep/.test(await p.evaluate(()=>document.getElementById('ltInfo').textContent)),'no warning after');
 // feed above roughing
 t=await set(4,{side:0,dstock:40,dfin:39,passes:1,feed:0.3},{mat:2,tm:0});ok(/0\.30 mm\/turn is more/.test(t),'feed warning');
 // parting: lower Vc, its own feed; facing uses the stock Ø
 t=await set(6,{side:0,dstock:30,dfinp:0,passes:1,feed:0.04},{mat:1,tm:0});ok(/Vc 72/.test(t)&&/feed 0\.03 - 0\.06/.test(t),'parting: '+t.slice(0,250));
 t=await set(5,{dstock:60,dfinf:0,zdep:2,passes:1,feed:0.08},{mat:1,tm:0});ok(/at Ø 60\.00/.test(t)&&/4 passes/.test(t),'facing Ø60, depth 2 -> 4 passes');
 // threads: nothing
 t=await set(7,{side:0},{});ok(!/rpm \(Vc/.test(t),'no advice for threads');
 // the material choice is kept
 await set(4,{side:0,dstock:50,dfin:46,passes:4,feed:0.1},{mat:0,tm:0});
 await p.selectOption('#ltInfo select[data-adv="mat"]','3');await p.waitForTimeout(200);
 ok(await p.evaluate(()=>ltV.mat==3&&JSON.parse(localStorage.getItem('h4lathe3')).mat==3),'material kept');
 await b.close();
 const all=fails.concat(errs.map(e=>'page error: '+e));all.forEach(f=>console.log('FAIL',f));
 console.log('advice: '+n+' checks, '+all.length+' failures');process.exit(all.length?1:0)})();
