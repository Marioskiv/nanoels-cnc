// Measure and correct (Lathe page, Turning): after a turning that ended, the measured Ø gives "on size", a correction
// pass (one pass to the final Ø, then the passes come back) or "too much off" with the Ø for the next part.
// Runs the page's demo in a real browser. Usage: node measure.js <page.html>
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:1280,height:800}});
 const errs=[];p.on('pageerror',e=>errs.push(String(e)));
 await p.goto('file://'+require('path').resolve(process.argv[2])+'?demo');await p.waitForTimeout(700);
 await p.evaluate(()=>{const t=[...document.querySelectorAll('[data-t]')].find(b=>/lathe/i.test(b.dataset.t));t.click()});await p.waitForTimeout(400);
 const fails=[];let n=0;const ok=(c,w)=>{n++;if(!c)fails.push(w)};
 const tap=sel=>p.evaluate(sel=>{const e=document.querySelector(sel);if(!e)throw new Error('missing '+sel);e.click()},sel);
 const info=()=>p.evaluate(()=>document.getElementById('ltInfo').textContent);
 const pad=async keys=>{for(const k of keys)await p.click(`.ltpad button[data-p="${k}"]`);await p.click('.ltpad button[data-p="o"]');await p.waitForTimeout(200)};
 const run=async()=>{await p.evaluate(()=>{ltGd={z:gdOff('z'),x:gdOff('x'),boot:st.boot};ltStartGo()});
  for(let i=0;i<120;i++){await p.waitForTimeout(250);if(await p.evaluate(()=>!st.on&&!ltJob&&ltWasOn===false&&!!ltV[4].meas||false))break}};
 const side=async(sd,ds,df)=>{await p.evaluate(([sd,ds,df])=>{ltOp=4;Object.assign(ltV[4],{side:sd,dstock:ds,dfin:df,za:0,len:10,passes:2,feed:0.1,cx:0,cz:0,meas:null});delete ltV[4].cor;ltForm()},[sd,ds,df])};
 // external: 25 -> 20 in 2 passes
 await side(0,25,20);await run();
 let me=await p.evaluate(()=>ltV[4].meas);ok(me&&me.fin==20&&me.d==null,'asks for the Ø after a normal end '+JSON.stringify(me));
 ok(/Μέτρησε|Measure/.test(await info())&&await p.$('#ltInfo [data-ms="m"]'),'measure button shown');
 await tap('#ltInfo [data-ms="m"]');await pad('20.06');
 let t=await info();ok(/0\.060/.test(t)&&await p.$('#ltInfo [data-ms="c"]'),'0.06 still to come off + correction button: '+t.slice(0,200));
 await tap('#ltInfo [data-ms="c"]');await p.waitForTimeout(200);
 let v=await p.evaluate(()=>({ds:ltV[4].dstock,df:ltV[4].dfin,ps:ltV[4].passes,cor:ltV[4].cor}));
 ok(v.ds==20.06&&v.df==20&&v.ps==1&&v.cor==2,'correction pass set: '+JSON.stringify(v));ok(/20\.060.*20\.000|20\.060/.test(await info()),'correction note');
 await run();v=await p.evaluate(()=>({ps:ltV[4].passes,cor:ltV[4].cor,me:ltV[4].meas}));
 ok(v.ps==2&&v.cor===undefined&&v.me&&v.me.d==null,'passes back after the correction, asks again: '+JSON.stringify(v));
 await tap('#ltInfo [data-ms="m"]');await pad('20.003');t=await info();ok(/στη διάσταση|on size/.test(t),'on size: '+t.slice(0,160));
 // too much off: suggests the final Ø for the next part
 await p.evaluate(()=>{ltV[4].meas.d=null;ltForm()});await tap('#ltInfo [data-ms="m"]');await pad('19.95');t=await info();
 ok(/0\.050/.test(t)&&/20\.050/.test(t)&&!(await p.$('#ltInfo [data-ms="c"]')),'too much off -> next part 20.050: '+t.slice(0,220));
 // internal (bore 20 -> 26): measured 25.9 -> 0.1 to come off
 await side(1,20,26);await run();await tap('#ltInfo [data-ms="m"]');await pad('25.9');t=await info();
 ok(/0\.100/.test(t)&&await p.$('#ltInfo [data-ms="c"]'),'bore 25.9: 0.1 to come off');
 await tap('#ltInfo [data-ms="c"]');v=await p.evaluate(()=>({ds:ltV[4].dstock,df:ltV[4].dfin,ps:ltV[4].passes}));ok(v.ds==25.9&&v.df==26&&v.ps==1,'bore correction '+JSON.stringify(v));
 // a changed final Ø hides the old measurement; a STOP asks nothing
 await p.evaluate(()=>{ltV[4].passes=2;delete ltV[4].cor;ltV[4].meas={fin:26,side:1,zr:0,zl:-10,d:25.9};ltV[4].dfin=25;ltForm()});ok(!/25\.900/.test(await info()),'other final Ø: not shown');
 const end=await p.evaluate(()=>{ltV[4].meas=null;msEnd({m:4,v:{dfin:20,side:0,cor:3},q:{zr:0,zl:-10},done0:5},{done:5});return {me:ltV[4].meas,ps:ltV[4].passes}});
 ok(end.me===null&&end.ps==3,'stopped job: nothing to measure, passes back '+JSON.stringify(end));
 // the part's drawing takes the measured Ø
 await side(0,25,20);await p.evaluate(()=>{partNew(25,40,0);ltGd={z:gdOff('z'),x:gdOff('x'),boot:st.boot}});await run();
 await tap('#ltInfo [data-ms="m"]');await pad('20.2');
 const r=await p.evaluate(()=>{const v=ltV[4],me=v.meas,a=Math.min(me.zr,me.zl)+partOff(),b=Math.max(me.zr,me.zl)+partOff();let rr=[];for(let i=0;i<ltPart.ro.length;i++){const z=partZ(i);if(z>a+0.05&&z<b-0.05)rr.push(ltPart.ro[i])}return rr});
 ok(r.length>0&&r.every(x=>Math.abs(x-10.1)<1e-9),'part radius = measured/2 over the length ('+r.length+')');
 await b.close();
 const all=fails.concat(errs.map(e=>'page error: '+e));all.forEach(f=>console.log('FAIL',f));
 console.log('measure: '+n+' checks, '+all.length+' failures');process.exit(all.length?1:0)})();
