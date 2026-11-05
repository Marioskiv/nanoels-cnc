// Settings (app 1.37): every page whole, scrolled with the finger (no ‹ 1/2 › pages); in the Android app a page
// "App log" shows how the last runs ended (from the app) and can be cleared. A stand-in for the app's H4App.
// Usage: node settings.js <page.html>
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const fails=[];let n=0;const ok=(c,w)=>{n++;if(!c)fails.push(w)};const errs=[];
 for(const app of [false,true]){
  const p=await b.newPage({viewport:{width:1024,height:600}});p.on('pageerror',e=>errs.push(String(e)));
  await p.addInitScript(([app])=>{localStorage.setItem('h4look',JSON.stringify({lang:'el'}));
   if(app){window.LOGTEXT='04/11 10:00 → 04/11 10:20 · app 1.36 · το έκλεισε το Android ΕΝΩ ΗΤΑΝ ΣΤΗΝ ΟΘΟΝΗ\n';window.H4App={settings(){},changeAddress(){},checkAppUpdate(){},checkFirmware(){},
    exitLog(){return window.LOGTEXT},clearExitLog(){window.LOGTEXT='-'},orientation(){return 'auto'},setOrientation(){},setLang(){},version(){return '1.37'}}}},[app]);
  await p.goto('file://'+require('path').resolve(process.argv[2])+'?demo');await p.waitForTimeout(700);
  await p.evaluate(()=>tab('settings'));
  for(const k of ['look','screen','pad','m0','wifi','fw']){await p.evaluate(k=>setShow(k),k);await p.waitForTimeout(150);
   const r=await p.evaluate(()=>{const pg=document.querySelector('.spage:not([hidden]) .pg'),c=pg.nextElementSibling;
    return {hidden:[...pg.children].filter(e=>e.hidden).length,pgc:!!(c&&c.classList.contains('pgc')&&!c.hidden),ov:getComputedStyle(pg).overflowY}});
   ok(r.hidden==0&&!r.pgc&&r.ov=='auto',(app?'app ':'')+k+': whole page, scrolls '+JSON.stringify(r))}
  // the gamepad page: all 17 buttons on it, and it really scrolls to the last one
  await p.evaluate(()=>setShow('pad'));await p.waitForTimeout(150);
  let r=await p.evaluate(()=>{const pg=document.querySelector('.spage[data-sp=pad] .pg');pg.scrollTop=1e6;const last=[...pg.querySelectorAll('.gpr')].pop().getBoundingClientRect(),box=pg.getBoundingClientRect();
   return {rows:pg.querySelectorAll('.gpr').length,scroll:pg.scrollHeight>pg.clientHeight,lastIn:last.bottom<=box.bottom+1&&last.top>=box.top-1}});
  ok(r.rows==17&&r.scroll&&r.lastIn,'gamepad page scrolls to its last button '+JSON.stringify(r));
  // the list itself
  r=await p.evaluate(()=>[...document.querySelectorAll('#slist button')].map(b=>b.dataset.k));
  ok(app?r.includes('applog'):!r.includes('applog'),'App log only in the app: '+r);
  if(app){await p.evaluate(()=>setShow('applog'));await p.waitForTimeout(150);
   r=await p.evaluate(()=>$('appLog').textContent);ok(/ΕΝΩ ΗΤΑΝ ΣΤΗΝ ΟΘΟΝΗ/.test(r),'app log shown: '+r);
   p.once('dialog',d=>d.accept());await p.evaluate(()=>[...document.querySelectorAll('.spage[data-sp=applog] button')].pop().click());await p.waitForTimeout(150);
   ok(await p.evaluate(()=>$('appLog').textContent=='-'),'app log cleared');
   ok(await p.evaluate(()=>/Ημερολόγιο app/.test($('sTitle').textContent)),'title in Greek')}
  await p.close()}
 await b.close();
 const all=fails.concat(errs.map(e=>'page error: '+e));all.forEach(f=>console.log('FAIL',f));
 console.log('settings: '+n+' checks, '+all.length+' failures');process.exit(all.length?1:0)})();
