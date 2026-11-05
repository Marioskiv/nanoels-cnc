// Choice lists (app 1.43): every <select> is shown as a button whose list rolls down under it; a second tap on the
// button closes the list (Android's own list is a centred window, and the tap that closed it opened it again).
// Usage: node dropdown.js <page.html>
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const fails=[];let n=0;const ok=(c,w)=>{n++;if(!c)fails.push(w)};const errs=[];
 const p=await b.newPage({viewport:{width:1024,height:600},hasTouch:true});p.on('pageerror',e=>errs.push(String(e)));
 await p.addInitScript(()=>localStorage.setItem('h4look',JSON.stringify({lang:'en'})));
 await p.goto('file://'+require('path').resolve(process.argv[2])+'?demo');await p.waitForTimeout(700);
 const open=()=>p.evaluate(()=>!$('ddList').hidden);
 // every select has its button; no native list is reachable
 let r=await p.evaluate(()=>[...document.querySelectorAll('select')].filter(s=>!s.multiple).map(s=>!!s._dd&&getComputedStyle(s).pointerEvents=='none'));
 ok(r.length>5&&r.every(x=>x),'every select has a button: '+r.length);
 await p.evaluate(()=>{tab('settings');setShow('look')});await p.waitForTimeout(200);
 const lang='#lkLang + .ddb';
 ok(/English/.test(await p.textContent(lang)),'button shows the value');
 await p.tap(lang);await p.waitForTimeout(100);ok(await open(),'tap opens the list');
 r=await p.evaluate(()=>{const a=$('lkLang')._dd.getBoundingClientRect(),l=$('ddList').getBoundingClientRect();return [l.top>=a.bottom,l.left>=0,l.right<=innerWidth,$('ddList').children.length]});
 ok(r[0]&&r[1]&&r[2]&&r[3]==2,'list under its button, on screen, 2 choices: '+r);
 await p.tap(lang);await p.waitForTimeout(100);ok(!await open(),'second tap closes it');
 await p.tap(lang);await p.waitForTimeout(100);ok(await open(),'third tap opens it again');
 await p.mouse.click(900,560);await p.waitForTimeout(100);ok(!await open(),'a tap elsewhere closes it');
 await p.tap(lang);await p.waitForTimeout(100);await p.tap('#ddList button:nth-child(2)');await p.waitForTimeout(300);
 r=await p.evaluate(()=>[look.lang,$('lkLang').value,$('ddList').hidden,$('lkLang')._dd.textContent]);
 ok(r[0]=='el'&&r[1]=='el'&&r[2]&&/Ελληνικά/.test(r[3]),'a choice sets the value, fires the change, closes: '+r);
 // a value set by the code shows on the button
 r=await p.evaluate(()=>{$('lkTheme').value='dark';return $('lkTheme')._dd.textContent});ok(/dark|σκούρο/i.test(r),'value set by code shows: '+r);
 // another screen closes an open list
 await p.tap('#lkTheme + .ddb');await p.evaluate(()=>tab('control'));await p.waitForTimeout(100);ok(!await open(),'changing screen closes it');
 // lists made later (gamepad page, lathe forms) get their button too
 await p.evaluate(()=>{tab('settings');setShow('pad')});await p.waitForTimeout(200);
 r=await p.evaluate(()=>{const s=document.querySelector('.spage[data-sp=pad] select[data-gb="l1"][data-gi="1"]');return [!!s._dd,s._dd.disabled]});
 ok(r[0]&&r[1],'gamepad lists have buttons, disabled stays disabled: '+r);
 ok(errs.length==0,'no page errors: '+errs.join(' | '));
 await b.close();
 for(const f of fails)console.log('FAIL',f);console.log(`dropdown: ${n} checks, ${fails.length} failures`);process.exit(fails.length?1:0)})();
