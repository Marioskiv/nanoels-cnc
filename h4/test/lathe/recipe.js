// Sequence of operations (recipe) of the Lathe page: build, run step by step (each START by the operator), tool
// change asks for the touch-offs, save / load, export / import. Runs the page's demo in a real browser.
// Usage: node recipe.js <page.html>
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
 const tap=sel=>p.evaluate(sel=>{const e=document.querySelector(sel);if(!e)throw new Error('missing '+sel);e.click()},sel);
 const pad=async keys=>{for(const k of String(keys))await p.click(`.ltpad button[data-p="${k}"]`);await p.click('.ltpad button[data-p="o"]');await p.waitForTimeout(150)};
 const info=()=>p.evaluate(()=>document.getElementById('ltInfo').textContent);
 const zeros=()=>p.evaluate(()=>{ltGd={z:gdOff('z'),x:gdOff('x'),boot:st.boot};gdSave();ltForm()});
 const run=async()=>{await zeros();await p.evaluate(()=>ltStartGo());
  for(let i=0;i<120;i++){await p.waitForTimeout(250);if(await p.evaluate(()=>!st.on&&!ltJob&&ltRec.fin))break}};
 // build: facing T1, turning T1, threading T2
 await p.evaluate(()=>{ltRec={steps:[],cur:-1,fin:false};ltOp=5;Object.assign(ltV[5],{dstock:30,dfinf:0,za:0,zdep:1,passes:2,feed:0.08,cx:0,cz:0});ltForm()});
 await tap('#ltForm [data-rc="open"]');await tap('[data-rk="add"]');await pad(1);
 await p.evaluate(()=>{sheetClose();ltOp=4;Object.assign(ltV[4],{side:0,dstock:30,dfin:24,za:0,len:10,passes:2,feed:0.1,cx:0,cz:0});ltForm()});
 await tap('#ltForm [data-rc="open"]');await tap('[data-rk="add"]');await pad(1);
 await p.evaluate(()=>{sheetClose();ltOp=7;Object.assign(ltV[7],{side:0,thr:'M24 × 2',za:-2,len:8,cx:0,cz:0,passes:2});ltThrUse(ltV[7]);ltForm()});
 await tap('#ltForm [data-rc="open"]');await tap('[data-rk="add"]');await pad(2);
 let r=await p.evaluate(()=>({n:ltRec.steps.length,ms:ltRec.steps.map(s=>s.m),ts:ltRec.steps.map(s=>s.t),thr:ltRec.steps[2].v.thr,rows:document.querySelectorAll('#shBody .rcrow').length}));
 ok(r.n==3&&r.ms.join()=='5,4,7'&&r.ts.join()=='1,1,2'&&r.thr=='M24 × 2'&&r.rows==3,'built '+JSON.stringify(r));
 // reorder and back
 await tap('[data-rk="down"][data-i="0"]');ok((await p.evaluate(()=>ltRec.steps.map(s=>s.m).join()))=='4,5,7','moved down');
 await tap('[data-rk="up"][data-i="1"]');ok((await p.evaluate(()=>ltRec.steps.map(s=>s.m).join()))=='5,4,7','moved up');
 // run: step 1 loaded into its form, nothing started
 await tap('[data-rk="run"]');await p.waitForTimeout(200);
 r=await p.evaluate(()=>({cur:ltRec.cur,op:ltOp,on:st.on,v:ltV[5].dstock}));ok(r.cur==0&&r.op==5&&!r.on&&r.v==30,'run loads step 1 '+JSON.stringify(r));
 ok(/1 (από|of) 3/.test(await info())&&!(await p.$('#ltInfo [data-rc="next"]')),'step 1 of 3, no Next before the end');
 await run();ok(await p.$('#ltInfo [data-rc="next"]'),'Next after the step ended');
 await tap('#ltInfo [data-rc="next"]');await p.waitForTimeout(200);
 r=await p.evaluate(()=>({cur:ltRec.cur,op:ltOp,d:ltV[4].dfin,g:gdState()}));ok(r.cur==1&&r.op==4&&r.d==24&&r.g.z==''&&r.g.x=='','step 2, same tool: zeros kept '+JSON.stringify(r));
 await run();
 // the turning asks for the measurement too; Next goes on
 ok(/Μέτρησε|Measure/.test(await info()),'measure prompt with the sequence');
 await tap('#ltInfo [data-rc="next"]');await p.waitForTimeout(200);
 r=await p.evaluate(()=>({cur:ltRec.cur,op:ltOp,thr:ltV[7].thr,g:gdState()}));ok(r.cur==2&&r.op==7&&r.thr=='M24 × 2'&&r.g.z=='need'&&r.g.x=='need','step 3, tool T2: touch-offs asked '+JSON.stringify(r));
 ok(/T2/.test(await info()),'tool shown');
 // START refused until the zeros; then the last step ends the sequence
 await p.evaluate(()=>ltStartGo());await p.waitForTimeout(300);ok(!(await p.evaluate(()=>st.on||!!ltJob)),'guard holds the new tool');
 await run();let t=await info();ok(/τελείωσε|finished/.test(t)&&!(await p.$('#ltInfo [data-rc="next"]')),'sequence finished');
 // a STOP keeps the step
 await p.evaluate(()=>{ltRec.cur=0;ltRec.fin=false;recEnd({m:5,done0:9},{done:9})});ok(await p.evaluate(()=>!ltRec.fin),'stopped step: no Next');
 await tap('#ltInfo [data-rc="end"]');ok(await p.evaluate(()=>ltRec.cur==-1),'sequence ended');
 // edit a step through the form
 await tap('#ltForm [data-rc="open"]');await tap('[data-rk="edit"][data-i="1"]');await p.evaluate(()=>{ltV[4].dfin=22});
 await tap('#ltForm [data-rc="open"]');await tap('[data-rk="put"]');ok(await p.evaluate(()=>ltRec.steps[1].v.dfin==22),'edited step saved');
 // save, empty, load
 await tap('[data-rk="save"]');r=await p.evaluate(()=>ltRecs[0].name);ok(/M24 × 2/.test(r),'saved with a name: '+r);
 await tap('[data-rk="clear"]');ok(await p.evaluate(()=>ltRec.steps.length==0),'emptied');
 await tap('[data-rk="load"][data-i="0"]');ok(await p.evaluate(()=>ltRec.steps.length==3&&ltRec.steps[1].v.dfin==22),'loaded');
 // export / import (and bad text)
 await tap('[data-rk="exp"]');const txt=await p.evaluate(()=>document.getElementById('rcText').value);ok(/"h4recipe":1/.test(txt),'exported');
 await tap('[data-rk="clear"]');await p.evaluate(t=>{document.getElementById('rcText').value=t},txt);await tap('[data-rk="imp"]');
 ok(await p.evaluate(()=>ltRec.steps.length==3&&ltRec.steps[2].v.thr=='M24 × 2'),'imported');
 r=await p.evaluate(()=>[recImport('nonsense'),recImport('{"steps":[{"m":99,"v":{}}]}'),recImport('{"steps":[{"m":4,"t":1,"v":{"dfin":"x","evil":"<b>","len":12}}]}')]);
 ok(typeof r[0]=='string'&&typeof r[1]=='string'&&Array.isArray(r[2])&&r[2][0].v.evil===undefined&&r[2][0].v.len==12&&isFinite(r[2][0].v.dfin),'import checks '+JSON.stringify(r[2]));
 await b.close();
 const all=fails.concat(errs.map(e=>'page error: '+e));all.forEach(f=>console.log('FAIL',f));
 console.log('recipe: '+n+' checks, '+all.length+' failures');process.exit(all.length?1:0)})();
