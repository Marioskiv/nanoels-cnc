// Builds the Lathe page plans (form -> values sent to the H4 -> drawn path) for compare.py, in a real browser.
// Usage: node plans.js <page.html> <out.json>  (page.html = the H4 page from webpage.h, opened with ?demo)
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
const scen=[
 [4,{side:0,dstock:25,dfin:20,za:0,len:30,passes:4,feed:0.1}],
 [4,{side:1,dstock:20,dfin:26,za:0,len:15,passes:3,feed:0.1}],
 [5,{dstock:30,dfinf:0,za:0,zdep:2,passes:4,feed:0.08}],
 [5,{dstock:30,dfinf:10,za:-1,zdep:1,passes:2,feed:0.08}],
 [6,{side:0,dstock:25,dfinp:0,passes:1,feed:0.04}],
 [6,{side:0,dstock:25,dfinp:10,passes:3,feed:0.04}],
 [6,{side:1,dstock:20,dfinp:26,passes:2,feed:0.04}],
 [7,{side:0,hand:0,dnom:10,dthr:8.16,za:-2,len:20,pitch:1.5,starts:1,passes:10}],
 [7,{side:0,hand:1,dnom:10,dthr:8.16,za:-2,len:20,pitch:1.5,starts:1,passes:10}],
 [7,{side:1,hand:0,dnom:8.376,dthr:10,za:-2,len:15,pitch:1.5,starts:2,passes:6}],
 [8,{shape:0,dstock:24,za:0,rz:12,rx:12,passes:12,feed:0.07}],
 [8,{shape:1,dstock:24,za:0,rz:6,rx:3,passes:5,feed:0.07}],
 [3,{side:0,dir:0,dbig:40,dsmall:30,len:20,feed:0.08}],
 [3,{side:0,dir:1,dbig:40,dsmall:30,len:20,feed:0.08}],
 [3,{side:1,big:1,dir:0,dbig:30,dsmall:20,len:20,feed:0.08}],
 [3,{side:0,big:1,dir:1,dbig:30,dsmall:20,len:20,feed:0.08}],
 [8,{shape:0,dstock:20,za:-1,rz:5,rx:4,passes:4,feed:0.07}],
 [4,{side:0,dstock:25,dfin:20,za:0,len:30,passes:4,feed:0.1}],
];
(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:1280,height:800}});
 await p.goto('file://'+require('path').resolve(process.argv[2])+'?demo');await p.waitForTimeout(700);
 await p.evaluate(()=>{const t=[...document.querySelectorAll('[data-t]')].find(b=>/lathe/i.test(b.dataset.t));t.click()});await p.waitForTimeout(400);
 const out=[];
 for(const [m,vals] of scen){
  const r=await p.evaluate(([m,vals])=>{ltOp=m;Object.assign(ltV[m],vals,{cx:0,cz:0});ltForm();const q=ltPlan();const err=ltCheck();const G=ltGeo();
    return {m,vals,err,q,st:{z:st.z,x:st.x},path:G.path,cuts:G.cuts.map(c=>({pass:c.pass,pts:c.pts})),xmarks:G.xmarks,label:G.label}},[m,vals]);
  out.push(r)}
 require('fs').writeFileSync(process.argv[3],JSON.stringify(out));console.log(out.length,'plans');await b.close()})();
