// The Lathe page without ready-made numbers (app 1.33): the forms start empty, START waits and says what to fill,
// the drawing waits, a measured Ø fills the part Ø, values kept by older versions are emptied once, and the drawing
// marks where the operation starts. Runs the page's demo in a real browser. Usage: node nodef.js <page.html>
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const fails=[];let n=0;const ok=(c,w)=>{n++;if(!c)fails.push(w)};const errs=[];
 const open=async(init)=>{const p=await b.newPage({viewport:{width:1280,height:800}});p.on('pageerror',e=>errs.push(String(e)));
  await p.addInitScript(i=>{localStorage.setItem('h4look',JSON.stringify({lang:'en'}));if(i)localStorage.setItem('h4lathe3',i)},init||'');
  await p.goto('file://'+require('path').resolve(process.argv[2])+'?demo');await p.waitForTimeout(700);
  await p.evaluate(()=>{[...document.querySelectorAll('[data-t]')].find(b=>/lathe/i.test(b.dataset.t)).click()});await p.waitForTimeout(300);
  await p.evaluate(()=>{ltPart={d:100,len:250,bore:0,boot:st.boot,z0:0-partOff(),ro:Array(2501).fill(50),ri:Array(2501).fill(0),thr:[],cut:0};partSave()});return p};
 // a first start: every operation's numbers empty, but the safe distances and the starts
 let p=await open();
 let r=await p.evaluate(()=>{const o={};for(const m in LTF)for(const f of LTF[m])if(f[1]=='n')o[m+'.'+f[0]]=ltV[m][f[0]];return o});
 ok(Object.entries(r).every(([k,v])=>/\.(cx|cz)$/.test(k)?v==5:/\.starts$/.test(k)?v==1:v===''),'forms empty: '+JSON.stringify(r));
 for(const m of [0,2,3,4,5,6,7,8]){const t=await p.evaluate(m=>{ltOp=m;ltGd={z:gdOff('z'),x:gdOff('x'),boot:st.boot};ltForm();ltDraw();return {info:document.getElementById('ltInfo').textContent,err:ltCheck(),path:ltGeo().path.length}},m);
  ok(/^Fill in: /.test(t.err)&&/Fill in/.test(t.info)&&t.path==0,'mode '+m+' waits: '+t.err)}
 await p.evaluate(()=>{ltOp=4;ltForm();ltStartGo()});await p.waitForTimeout(400);ok(!(await p.evaluate(()=>st.on||!!ltJob)),'START refused while empty');
 // an empty Z place: the arrow starts from where the tool stands; a measured Ø fills the part Ø
 await p.evaluate(()=>{document.querySelector('#ltForm [data-k="za"][data-zd]').click()});ok(await p.evaluate(()=>ltV[4].za!==''&&Math.abs(ltV[4].za-(+(+st.z).toFixed(1)+1))<1e-9||Math.abs(ltV[4].za-(+(+st.z).toFixed(1)-1))<1e-9),'arrow on an empty Z starts from the tool');
 await p.click('#ltForm [data-z="dia"]');for(const k of '32')await p.click(`.ltpad button[data-p="${k}"]`);await p.click('.ltpad button[data-p="o"]');await p.waitForTimeout(1800);
 r=await p.evaluate(()=>[ltV[4].dstock,ltV[5].dstock,ltV[6].dstock,ltV[8].dstock,ltV[7].dnom]);ok(r.every(x=>x==32),'measured Ø fills the part Ø: '+r);
 // filled: the drawing has the path and the start mark (green flag) at its first cut
 await p.evaluate(()=>{Object.assign(ltV[4],{za:0,len:20,dfin:28,passes:2,feed:0.1});ltForm();ltDraw()});
 r=await p.evaluate(()=>({err:ltCheck(),svg:document.getElementById('ltSvg').innerHTML}));ok(r.err===''&&/stroke="var\(--ok\)" fill="var\(--ok\)"/.test(r.svg)&&/>Start</.test(r.svg),'start mark drawn');
 await p.close();
 // values kept by an older version are emptied once, the safe distances stay; later values stay
 p=await open(JSON.stringify({4:{za:-11,len:58,dstock:25,dfin:20,cx:3,cz:4,passes:8,feed:0.1,side:0}}));
 r=await p.evaluate(()=>ltV[4]);ok(r.za===''&&r.len===''&&r.dstock===''&&r.cx==3&&r.cz==4&&r.side==0,'old values emptied once: '+JSON.stringify(r));
 ok(await p.evaluate(()=>{ltV[4].len=12;ltSave();const k=JSON.parse(localStorage.getItem('h4lathe3'));return k.nodef==1&&k[4].len==12}),'emptied only once: new values kept');
 await b.close();
 const all=fails.concat(errs.map(e=>'page error: '+e));all.forEach(f=>console.log('FAIL',f));
 console.log('nodef: '+n+' checks, '+all.length+' failures');process.exit(all.length?1:0)})();
