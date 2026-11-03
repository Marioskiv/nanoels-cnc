// The Lathe page's Z and the drawing (app 1.35): Z in the forms is mm from the face towards the chuck, positive, in
// any Z view; the operations on the part wait for the Part; with a Part the drawing keeps its scale (a changed value
// moves the cut) and names the face, the jaws and the centre. Runs the page's demo in a real browser.
// Usage: node zface.js <page.html>
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const fails=[];let n=0;const ok=(c,w)=>{n++;if(!c)fails.push(w)};const errs=[];
 const p=await b.newPage({viewport:{width:1280,height:800}});p.on('pageerror',e=>errs.push(String(e)));
 await p.addInitScript(()=>{localStorage.setItem('h4look',JSON.stringify({lang:'en'}))});
 await p.goto('file://'+require('path').resolve(process.argv[2])+'?demo');await p.waitForTimeout(700);
 await p.evaluate(()=>{[...document.querySelectorAll('[data-t]')].find(b=>/lathe/i.test(b.dataset.t)).click()});await p.waitForTimeout(300);
 // no Part: turning asks for it (form, START, drawing, the Part button marked)
 let r=await p.evaluate(()=>{ltPart=null;partSave();ltOp=4;ltGd={z:gdOff('z'),x:gdOff('x'),boot:st.boot};Object.assign(ltV[4],{side:0,za:10,len:30,dstock:30,dfin:24,passes:3,feed:0.1,cx:0,cz:0});ltForm();ltDraw();
  return {err:ltCheck(),svg:document.getElementById('ltSvg').innerHTML,need:document.querySelector('.ltpartb').classList.contains('need')}});
 ok(/Part/.test(r.err)&&/Part/.test(r.svg)&&r.need,'no Part: asked for '+JSON.stringify(r).slice(0,200));
 await p.evaluate(()=>ltStartGo());await p.waitForTimeout(300);ok(!(await p.evaluate(()=>st.on||!!ltJob)),'START refused without a Part');
 // the Part from its sheet: Ø 30, 60 out of the jaws
 await p.click('.ltpartb');await p.waitForTimeout(200);
 const pad=async(k,txt)=>{await p.click(`#shBody [data-pk="${k}"]`);for(const c of txt)await p.click(`.ltpad button[data-p="${c}"]`);await p.click('.ltpad button[data-p="o"]')};
 await pad('d','30');await pad('len','60');await pad('bore','0');await p.click('#shBody [data-pk="new"]');await p.waitForTimeout(300);
 r=await p.evaluate(()=>({d:ltPart&&ltPart.d,len:ltPart&&ltPart.len,err:ltCheck()}));ok(r.d==30&&r.len==60&&r.err==='','Part set, turning ready: '+JSON.stringify(r));
 // Z in the form: positive mm from the face, shown so in both Z views; the pad keeps what was typed
 for(const zv of [0,1]){r=await p.evaluate(zv=>{st.lzv=zv;ltViewChanged();ltLast={};ltForm();return document.querySelector('#ltForm .numv[data-k="za"]').textContent},zv);
  ok(r=='10.000','start shown as 10 from the face (Z view '+zv+'): '+r)}
 await p.click('#ltForm .numv[data-k="za"]');for(const c of '15')await p.click(`.ltpad button[data-p="${c}"]`);await p.click('.ltpad button[data-p="o"]');
 r=await p.evaluate(()=>({za:ltV[4].za,q:ltPlan()}));ok(r.za==15&&r.q.zr==15&&r.q.zl==45,'typed 15: the H4 gets Z 15 … 45 (its frame grows towards the chuck): '+JSON.stringify(r.q).slice(0,120));
 // ◀ goes towards the chuck (+), ▶ towards the face (−)
 await p.click('#ltForm [data-k="za"][data-zd="1"]');ok(await p.evaluate(()=>ltV[4].za==16),'◀ adds 1 mm (towards the chuck)');
 await p.click('#ltForm [data-k="za"][data-zd="-1"]');ok(await p.evaluate(()=>ltV[4].za==15),'▶ takes 1 mm off (towards the face)');
 // the drawing keeps its scale while the values change, and the cut moves
 const look=()=>p.evaluate(()=>{ltDraw();const s=document.getElementById('ltSvg').innerHTML,m=s.match(/<polygon points="([^"]+)" fill="#ff7a2e"/);return {S:ltVw.S,cut:m?m[1]:'',svg:s}});
 const a=await look();await p.evaluate(()=>{ltSet(4,'len',20)});const b2=await look();await p.evaluate(()=>{ltSet(4,'za',25)});const c=await look();
 ok(Math.abs(a.S-b2.S)<1e-9&&Math.abs(a.S-c.S)<1e-9,'same scale: '+[a.S,b2.S,c.S]);
 ok(a.cut&&b2.cut&&c.cut&&a.cut!=b2.cut&&b2.cut!=c.cut,'the cut moves with the values');
 ok(/Z0 face/.test(a.svg)&&/>jaws</.test(a.svg)&&/X0 centre/.test(a.svg),'face, jaws and centre named');
 // the H4's display and the drawing's lines stay as lathes show them (ISO: Z −25)
 await p.evaluate(()=>{st.lzv=0;ltViewChanged();ltDraw()});r=await look();ok(/Z -25\.0/.test(r.svg),'line at Z -25.0 in the ISO view');
 // a stop (feed) is mm from the face too
 r=await p.evaluate(()=>{ltOp=0;Object.assign(ltV[0],{feed:0.1,dir:0,zstop:12});ltForm();return [document.querySelector('#ltForm .numv[data-k="zstop"]').textContent,ltPlan().zl,ltCheck()]});
 ok(r[0]=='12.000'&&r[1]==12&&r[2]==='','feed stop 12 from the face, no Part needed: '+r);
 await b.close();
 const all=fails.concat(errs.map(e=>'page error: '+e));all.forEach(f=>console.log('FAIL',f));
 console.log('zface: '+n+' checks, '+all.length+' failures');process.exit(all.length?1:0)})();
