// The Lathe page's drawing: the part as a half section (hatched below the centre line, outside view above, hidden
// bore, thread teeth) and the tool shape of each operation (inside tools along Z). Usage: node draw.js <page.html>
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:1280,height:800}});
 const errs=[];p.on('pageerror',e=>errs.push(String(e)));
 await p.goto('file://'+require('path').resolve(process.argv[2])+'?demo');await p.waitForTimeout(700);
 await p.evaluate(()=>{const t=[...document.querySelectorAll('[data-t]')].find(b=>/lathe/i.test(b.dataset.t));t.click()});await p.waitForTimeout(400);
 const fails=[];let n=0;const ok=(c,w)=>{n++;if(!c)fails.push(w)};
 const svg=()=>p.evaluate(()=>document.getElementById('ltSvg').innerHTML);
 // no part: hatched block below, outside view above
 let s=await p.evaluate(()=>{ltPart=null;ltOp=4;ltForm();ltDraw();return document.getElementById('ltSvg').innerHTML});
 ok(/pattern id="ltHatch"/.test(s)&&/url\(#ltHatch\)/.test(s),'hatched section without a part');
 // a tube with a thread: section polygon, outside half, hidden bore, teeth
 s=await p.evaluate(()=>{partNew(40,60,20);ltPart.thr.push({zr:0-partOff(),zl:20-partOff(),a:20,b:18.773,p:2,ext:true});partSave();ltVw.mode='all';ltForm();ltDraw();return document.getElementById('ltSvg').innerHTML});
 ok((s.match(/url\(#ltHatch\)/g)||[]).length>=1&&/stroke-dasharray="5 4"/.test(s)&&/<polyline/.test(s),'tube: section, hidden bore, thread teeth');
 // tool shapes
 const tool=m=>p.evaluate(m=>ltToolSvg(m,200),m);
 await p.evaluate(()=>{ltV[4].side=0;ltV[7].side=0;ltV[7].thr='M20 × 2.5';ltV[6].side=0});
 ok(/L-6 18 L14 22/.test(await tool(4)),'turning: 80° insert');
 const t60=await tool(7);ok(/L-9\.2\d* 16/.test(t60),'thread 60°: V '+t60.slice(0,60));
 await p.evaluate(()=>{ltV[7].thr='Tr20 × 4'});ok(/L-4\.2\d* 16/.test(await tool(7)),'Tr 30° narrower V');
 ok(/width="6" height="34"/.test(await tool(6)),'parting blade');ok(/<circle/.test(await tool(8)),'sphere round insert');
 await p.evaluate(()=>{ltV[4].side=1;ltV[7].side=1;ltV[7].thr='M20 × 2.5'});
 let ti=await tool(4);ok(/width="200" height="11"/.test(ti)&&/y="-26"/.test(ti),'bore: bar along Z');
 ti=await tool(7);ok(/width="200" height="11"/.test(ti)&&/-16 L/.test(ti),'inside thread: bar + V');
 // the drawing uses the tool of the operation
 s=await p.evaluate(()=>{ltOp=4;ltForm();ltDraw();return document.getElementById('ltSvg').innerHTML});ok(/height="11" rx="2"/.test(s),'drawing shows the boring bar');
 await b.close();
 const all=fails.concat(errs.map(e=>'page error: '+e));all.forEach(f=>console.log('FAIL',f));
 console.log('draw: '+n+' checks, '+all.length+' failures');process.exit(all.length?1:0)})();
