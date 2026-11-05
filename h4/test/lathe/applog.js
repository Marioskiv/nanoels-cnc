// App log (app 1.44): the page writes its steps to the app's full log (screens, commands, messages, the H4 link,
// page errors), says every second that it is alive, and Settings, App log shows the full log or how runs ended,
// with Copy and Send. A stand-in for the app's H4App collects the lines.
// Usage: node applog.js <page.html>
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const fails=[];let n=0;const ok=(c,w)=>{n++;if(!c)fails.push(w)};const errs=[];
 const p=await b.newPage({viewport:{width:1024,height:600}});p.on('pageerror',e=>errs.push(String(e)));
 await p.addInitScript(()=>{localStorage.setItem('h4look',JSON.stringify({lang:'el'}));window.LINES=[];window.ALIVE=[];
  window.H4App={settings(){},changeAddress(){},checkAppUpdate(){},checkFirmware(){},setLang(){},version(){return '1.44'},orientation(){return 'auto'},setOrientation(){},
   deviceStatus(){return '{"battery":50,"charging":false,"wifi":3}'},fullscreen(){return true},setFullscreen(){},openAtBoot(){return true},setOpenAtBoot(){},
   exitLog(){return 'RUNS'},clearExitLog(){window.LINES=[]},fullLog(){return 'FULL:'+window.LINES.length},copyLog(){window.COPIED=1;return true},shareLog(){window.SHARED=1},
   log(t,x){window.LINES.push(t+' '+x)},alive(w){window.ALIVE.push(w)}}});
 // the page's own requests: /status fails (no H4 in this test), commands answer ok
 await p.route('**/*',r=>{const u=r.request().url();if(u.startsWith('file:'))return r.continue();
  if(/\/status/.test(u))return r.abort();return r.fulfill({status:200,body:'ok'})});
 await p.goto('file://'+require('path').resolve(process.argv[2]));await p.waitForTimeout(2500);
 let L=await p.evaluate(()=>window.LINES);
 ok(L.some(l=>/^H4 ΧΑΘΗΚΕ Η ΣΥΝΔΕΣΗ/.test(l)),'link lost written once, in Greek: '+L.filter(l=>/^H4/.test(l)).length);
 ok(L.filter(l=>/^H4 ΧΑΘΗΚΕ/.test(l)).length==1,'link lost only once while it stays lost');
 ok((await p.evaluate(()=>window.ALIVE.length))>=2,'the page says it is alive every second');
 await p.evaluate(()=>tab('lathe'));await p.evaluate(()=>tab('settings'));
 L=await p.evaluate(()=>window.LINES);ok(L.includes('SCREEN lathe')&&L.includes('SCREEN settings'),'screens written');
 await p.waitForTimeout(1200);ok(/^settings/.test(await p.evaluate(()=>window.ALIVE[window.ALIVE.length-1])),'alive says where it is');
 await p.evaluate(()=>msg('Test message'));await p.evaluate(()=>post('/cmd?do=stop').catch(()=>{}));await p.waitForTimeout(300);
 L=await p.evaluate(()=>window.LINES);ok(L.includes('MSG Test message')&&L.includes('CMD /cmd?do=stop'),'messages and commands written');
 await p.evaluate(()=>post('/jog?x=1&z=0&c=0').catch(()=>{}));await p.waitForTimeout(200);
 L=await p.evaluate(()=>window.LINES.filter(l=>/jog\?/.test(l)));ok(L.length==0,'the jog repeats are not written: '+L);
 await p.evaluate(()=>setTimeout(()=>{throw new Error('boom')}));await p.waitForTimeout(200);
 ok((await p.evaluate(()=>window.LINES)).some(l=>/^JS ΣΦΑΛΜΑ.*boom/.test(l)),'page errors written');
 // Settings, App log: full log first, then how it closed; Copy and Send
 await p.evaluate(()=>setShow('applog'));await p.waitForTimeout(300);
 ok(/^FULL:/.test(await p.textContent('#appLog')),'full log shown first');
 await p.click('#alSeg button[data-v=runs]');ok(await p.textContent('#appLog')=='RUNS','how it closed shown');
 await p.click('#alCopy');await p.click('#alSend');
 ok(await p.evaluate(()=>window.COPIED==1&&window.SHARED==1),'copy and send reach the app');
 ok(errs.filter(e=>!/boom/.test(e)).length==0,'no page errors: '+errs.join(' | '));
 await b.close();
 for(const f of fails)console.log('FAIL',f);console.log(`applog: ${n} checks, ${fails.length} failures`);process.exit(fails.length?1:0)})();
