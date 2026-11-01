// Thread data of the Lathe page (ISO metric tolerances, own thread, G, Tr, UN) checked against hand-worked values.
// Usage: node threads.js <page.html>   (the H4 page of the app, opened with ?demo)
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:1280,height:800}});
 const errs=[];p.on('pageerror',e=>errs.push(String(e)));
 await p.goto('file://'+require('path').resolve(process.argv[2])+'?demo');await p.waitForTimeout(700);
 const r=await p.evaluate(()=>{const out=[];let n=0;
  const eq=(got,want,what,tol=0.0005)=>{n++;if(!(Math.abs(got-want)<=tol))out.push(what+': got '+got+', want '+want)};
  const ok=(c,what)=>{n++;if(!c)out.push(what)};
  const row=name=>LTTHR.find(t=>t[0]==name);
  // ISO 965-1 6g / 6H, M10 x 1.5 (GUIDE.md example) and M6 x 1
  let t=ltThr(row('M10 × 1.5'),false);eq(t.hi,9.968,'M10 6g max');eq(t.lo,9.732,'M10 6g min');eq(t.fin,8.160,'M10 root');
  t=ltThr(row('M10 × 1.5'),true);eq(t.lo,8.376,'M10 6H D1 min');eq(t.hi,8.676,'M10 6H D1 max');eq(t.fin,10,'M10 nut major');eq(t.drill,8.5,'M10 drill');
  t=ltThr(row('M6 × 1'),false);eq(t.hi,5.974,'M6 6g max');eq(t.lo,5.794,'M6 6g min');
  // the formulas give the standard's table within a few µm (and 4 %) at every pitch of the table
  for(const P in LTISO){const f=ltIsoForm(+P),tb=LTISO[P];for(let i=0;i<3;i++)ok(Math.abs(f[i]-tb[i])<=Math.max(6,Math.abs(tb[i])*0.04),'formula vs table P'+P+' #'+i+': '+f[i]+' / '+tb[i])}
  // own thread M20 x 1.25 (pitch in the table): es -28, Td 212, TD1 265
  const v={thr:'c',cust:{D:20,P:1.25}};t=ltThr(ltThrRow(v),false);eq(t.hi,19.972,'own M20x1.25 max');eq(t.lo,19.760,'own min');ok(t.own&&t.iso,'own is ISO');
  t=ltThr(ltThrRow(v),true);eq(t.lo,20-1.0825*1.25,'own D1',0.001);eq(t.hi-t.lo,0.265,'own TD1');
  // own thread with a pitch not in the table: from the formulas
  t=ltThr(ltThrRow({thr:'c',cust:{D:17,P:1.1}}),false);const f=ltIsoForm(1.1);eq(t.hi,17+f[0]/1000,'own P1.1 max');eq(t.hi-t.lo,f[1]/1000,'own P1.1 Td');
  ok(ltThrRow({thr:'c',cust:{D:0,P:1}})===null&&ltThrRow({thr:'c'})===null&&ltThrRow({thr:''})===null,'no row for empty / bad own thread');
  // G 1/2 (55°): d1 = d - 1.280654 P
  t=ltThr(row('G1/2'),false);eq(t.fin,20.955-1.280654*25.4/14,'G1/2 root',0.001);eq(t.mid,20.955,'G1/2 part');ok(t.ang==55&&!t.iso,'G 55° nominal');
  t=ltThr(row('G1/2'),true);eq(t.mid,20.955-1.280654*25.4/14,'G1/2 hole',0.001);eq(t.fin,20.955,'G1/2 nut major');
  // Tr ISO 2904: male d3 = d - P - 2 ac, nut D4 = d + 2 ac, hole D1 = d - P
  t=ltThr(row('Tr20 × 4'),false);eq(t.fin,15.5,'Tr20x4 d3');t=ltThr(row('Tr20 × 4'),true);eq(t.fin,20.5,'Tr20x4 D4');eq(t.mid,16,'Tr20x4 D1');
  eq(ltThr(row('Tr40 × 7'),false).fin,32,'Tr40x7 d3');eq(ltThr(row('Tr8 × 1.5'),false).fin,6.2,'Tr8x1.5 d3');ok(ltThr(row('Tr10 × 2'),false).ang==30,'Tr 30°');
  // UN 60°: 1/4-20 nominal, root d - 1.22687 P
  t=ltThr(row('1/4-20 UNC'),false);eq(t.mid,6.35,'1/4-20 part');eq(t.fin,6.35-1.22687*1.27,'1/4-20 root',0.001);ok(!t.iso,'UNC nominal');
  // the list: unique names, every group filled, every name of the old list still there (saved forms keep it)
  ok(new Set(LTTHR.map(t=>t[0])).size==LTTHR.length,'unique names');for(const g of ['mc','mf','uc','uf','g','t'])ok(LTTHR.some(t=>t[4]==g),'group '+g);
  for(const nm of LTTHR_OLD)ok(!!row(nm),'old name kept: '+nm);
  for(const t of LTTHR){for(const int of [false,true]){const x=ltThr(t,int);ok(x.lo<=x.hi&&(int?x.fin>x.hi:x.fin<x.lo),'order '+t[0]+(int?' F':' M'))}}
  // a form saved by app 1.22 (a place in the old list) becomes the name
  ltV[7].thr=6;ltFill();ok(ltV[7].thr==='M10 × 1.5','old place 6 -> M10 × 1.5: '+ltV[7].thr);
  // the form: list, then the own thread through the keypad
  ltOp=7;ltV[7].side=0;ltV[7].thr='M12 × 1.75';ltThrUse(ltV[7]);ltForm();const sel=document.getElementById('ltPre');
  ok(sel.options[sel.selectedIndex].text=='M12 × 1.75','list shows the choice');eq(ltV[7].pitch,1.75,'pitch from the list');eq(ltV[7].dthr,12-1.22687*1.75,'final Ø from the list',0.001);
  ok(/6g/.test(document.getElementById('ltInfo').textContent),'info shows 6g');
  sel.value='c';sel.dispatchEvent(new Event('change',{bubbles:true}));
  const pad=k=>[...document.querySelectorAll('.ltpad button')].find(b=>b.dataset.p==k).click();
  for(const k of '20')pad(k);pad('o');for(const k of '1.25')pad(k);pad('o');
  ok(ltV[7].thr==='c'&&ltV[7].cust.D==20&&ltV[7].cust.P==1.25,'own thread saved');eq(ltV[7].pitch,1.25,'own pitch');eq(ltV[7].dnom,(19.972+19.760)/2,'own aim');
  const info=document.getElementById('ltInfo').textContent;ok(/ISO 965-1/.test(info)&&/19\.760/.test(info),'own info: '+info.slice(0,120));
  // Preparation opens Turning at the middle of the tolerance
  document.querySelector('#ltForm [data-prep]').click();ok(ltOp==4&&Math.abs(ltV[4].dfin-ltV[7].dnom)<1e-9,'preparation -> turning');
  ok(/6g/.test(document.getElementById('ltInfo').textContent),'turning shows the class');
  // a nominal thread's preparation: no tolerance flag on the exact size
  ltOp=7;ltV[7].thr='G1/2';ltThrUse(ltV[7]);ltForm();document.querySelector('#ltForm [data-prep]').click();
  ok(!document.querySelector('#ltInfo .bad'),'G preparation not flagged');
  return {n,out}});
 await b.close();
 const fails=r.out.concat(errs.map(e=>'page error: '+e));fails.forEach(f=>console.log('FAIL',f));
 console.log('threads: '+r.n+' checks, '+fails.length+' failures');process.exit(fails.length?1:0)})();
