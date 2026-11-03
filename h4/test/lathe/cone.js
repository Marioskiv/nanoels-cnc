// Taper by angle and chamfer (Lathe page): Ø small from the half angle, chamfer start on the same line 1 mm in front of
// the face, the plan the H4 gets, and the move to the start. Runs the page's demo. Usage: node cone.js <page.html>
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
 const fails=[];let n=0;const ok=(c,w)=>{n++;if(!c)fails.push(w)};const near=(a,b,t=1e-3)=>Math.abs(a-b)<=t;
 const tap=sel=>p.evaluate(sel=>{const e=document.querySelector(sel);if(!e)throw new Error('missing '+sel);e.click()},sel);
 const pad=async keys=>{await p.waitForSelector('.ltpad');
  for(const k of String(keys))await p.click(`.ltpad button[data-p="${k}"]`);await p.click('.ltpad button[data-p="o"]');await p.waitForTimeout(150)};
 await p.evaluate(()=>{ltGd={z:gdOff('z'),x:gdOff('x'),boot:st.boot};gdSave();ltOp=3;Object.assign(ltV[3],{side:0,big:0,dir:0,dbig:40,dsmall:30,len:20,feed:0.08});delete ltV[3].ch;ltForm()});
 let t=await p.evaluate(()=>document.getElementById('ltInfo').textContent);ok(/Half angle α 14\.036/.test(t),'half angle of 40/30/20: '+t.slice(0,300));
 // angle 5° -> Ø small = 40 - 2*20*tan5
 await tap('#ltInfo [data-cn="ang"]');await pad('5');let v=await p.evaluate(()=>ltV[3].dsmall);ok(near(v,40-40*Math.tan(5*Math.PI/180),1e-3),'Ø small from 5°: '+v);
 // Morse-like: 1.4321° -> 1:20 on Ø
 // external chamfer 1 x 45 on Ø30
 await p.evaluate(()=>{Object.assign(ltV[3],{dbig:30});delete ltV[3].ch;ltForm()});
 await tap('#ltInfo [data-cn="ch"]');await pad('1');await pad('45');
 v=await p.evaluate(()=>({v:ltV[3],q:ltPlan()}));
 ok(near(v.v.dsmall,26)&&near(v.v.dbig,30)&&near(v.v.len,2)&&v.v.big==0&&v.v.dir==0&&v.v.ch&&near(v.v.ch.d,26),'chamfer ext values '+JSON.stringify(v.v));
 ok(near(v.q.cone,2)&&v.q.pitch>0&&v.q.aux==1,'plan: k 2 towards the chuck, Ø growing '+JSON.stringify(v.q));
 // the line: from (Z +1 in front, Ø26) it passes Ø28 at the face and Ø30 at 1 mm into the part
 const line=(z0,x0,q,len,z)=>{const dx=(q.aux?-1:1)*q.cone/2*(z-z0);return -2*(x0+dx)};
 ok(near(line(-1,-13,v.q,2,0),28)&&near(line(-1,-13,v.q,2,1),30),'chamfer line: Ø28 at the face, Ø30 at 1 mm');
 // internal (hole Ø20): 1.5 x 30°
 await p.evaluate(()=>{Object.assign(ltV[3],{side:1,dsmall:20,dbig:30});delete ltV[3].ch;ltForm()});
 await tap('#ltInfo [data-cn="ch"]');await pad('1.5');await pad('30');
 v=await p.evaluate(()=>({v:ltV[3],q:ltPlan()}));const tt=Math.tan(Math.PI/6);
 ok(near(v.v.dsmall,20)&&near(v.v.dbig,20+3+2*tt,1e-3)&&near(v.v.len,1.5/tt+1,1e-3)&&v.v.big==1&&v.q.aux==0,'chamfer int '+JSON.stringify(v.v));
 ok(near(line(-1,-v.v.dbig/2,v.q,v.v.len,0),23,1e-3),'internal: Ø23 at the mouth');
 // a changed value forgets the chamfer
 await p.evaluate(()=>{ltSet(3,'len',5)});ok(await p.evaluate(()=>!ltV[3].ch),'edit forgets the chamfer');
 // move to the start: refused while the tool is not in front of the face; then X, Z
 await p.evaluate(()=>{Object.assign(ltV[3],{side:0,dbig:30,len:20});ltForm()});await tap('#ltInfo [data-cn="ch"]');await pad('1');await pad('45');
 await p.evaluate(()=>post('/lathe?do=goto&axis=z&v=3'));await p.waitForTimeout(1500);
 await tap('#ltInfo [data-cn="go"]');await p.waitForTimeout(1500);ok(await p.evaluate(()=>!(Math.abs(st.z+1)<0.01)),'not moved from behind the face');
 await p.evaluate(()=>post('/lathe?do=goto&axis=z&v=-6'));await p.waitForTimeout(2500);
 await tap('#ltInfo [data-cn="go"]');for(let i=0;i<40;i++){await p.waitForTimeout(250);if(await p.evaluate(()=>Math.abs(st.z+1)<0.01))break}
 v=await p.evaluate(()=>({x:st.x,z:st.z}));ok(near(v.x,-13,0.01)&&near(v.z,-1,0.01),'tool at the chamfer start '+JSON.stringify(v));
 await b.close();
 const all=fails.concat(errs.map(e=>'page error: '+e));all.forEach(f=>console.log('FAIL',f));
 console.log('cone: '+n+' checks, '+all.length+' failures');process.exit(all.length?1:0)})();
