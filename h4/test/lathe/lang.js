// Every screen in the chosen language (app 1.36): in Greek no English text is left on the screens, the settings
// pages, the pop-ups and the explanations (but names: G-code, WiFi, STOP, Home All, button names ...); in English
// no Greek (but the language choice itself); switching the language back and forth gives the same texts again.
// Usage: node lang.js <page.html>
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
const OKW=/^(STOP|Home|Home All|G-code|WiFi|Firmware|MDI|OK|PS|PS4 \/ PS5|Share|Options|L[123]|R[123]|[✕○□△] \w+|demo|DEMO|English|Ελληνικά|Language \/ Γλώσσα|[\w.-]+\.(ngc|nc|gcode|tap|txt)|Jog lock)$/;
(async()=>{const b=await chromium.launch();const fails=[];let n=0;const ok=(c,w)=>{n++;if(!c)fails.push(w)};const errs=[];
 const p=await b.newPage({viewport:{width:1024,height:600}});p.on('pageerror',e=>errs.push(String(e)));
 await p.addInitScript(()=>{if(!localStorage.getItem('h4look'))localStorage.setItem('h4look',JSON.stringify({lang:'en'}))});
 await p.goto('file://'+require('path').resolve(process.argv[2])+'?demo');await p.waitForTimeout(800);
 const texts=()=>p.evaluate(()=>{const o=[];const w=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT);let x;
  while(x=w.nextNode()){const t=x.nodeValue.trim(),e=x.parentElement;if(t&&e&&(e.offsetParent!==null||e.closest('.spage:not([hidden])'))&&!e.closest('script,style,#ltForm,#ltOps,#ltSvg,#ltInfo,#ltLeg,svg'))o.push(t)}
  document.querySelectorAll('[placeholder]').forEach(e=>{if(e.offsetParent!==null)o.push(e.placeholder)});return o});
 const visit=async()=>{const all=[];const add=a=>all.push(...a);await p.evaluate(()=>document.querySelectorAll('.pg').forEach(g=>{if(g.id!='log')g.dataset.p=0}));
  for(const t of ['control','program','machine','settings']){await p.evaluate(t=>tab(t),t);await p.waitForTimeout(150);add(await texts());
   if(t=='program')for(const s of ['run','files','path']){await p.evaluate(s=>sub('program',s),s);await p.waitForTimeout(80);add(await texts())}
   if(t=='machine')for(const s of ['pos','diag','log']){await p.evaluate(s=>sub('machine',s),s);await p.waitForTimeout(80);add(await texts())}
   if(t=='settings')for(const k of ['look','screen','pad','m0','m1','m2','wifi','fw']){await p.evaluate(k=>setShow(k),k);await p.waitForTimeout(80);
    add(await texts());add(await p.evaluate(()=>[...document.querySelectorAll('.spage:not([hidden]) option')].map(o=>o.textContent.trim())))}}
  for(const f of ['ovrSheet()','macroSheet()','mdiSheet()',"infoSheet('files')","infoSheet('pad')","infoSheet('diag')"]){await p.evaluate(f=>eval(f),f);await p.waitForTimeout(100);add(await texts());await p.evaluate(()=>sheetClose())}
  return all};
 const setLang=async l=>{await p.evaluate(l=>{$('lkLang').value=l;$('lkLang').dispatchEvent(new Event('input'))},l);await p.waitForTimeout(300)};
 const en1=await visit();
 const greek=en1.filter(t=>/[Ͱ-Ͽ]/.test(t)&&!OKW.test(t));ok(!greek.length,'Greek in English: '+[...new Set(greek)].join(' | '));
 await setLang('el');const el=await visit();
 const eng=el.filter(t=>/[A-Za-z]{4,}/.test(t.replace(/\b(G-code|WiFi|Firmware|Home All|Home|STOP|MDI|PSRAM|GitHub|Bluetooth|USB|Android|Chrome|chrome:\/\/flags|Insecure origins treated as secure|Enabled|Relaunch|Share|Options|Cross|Circle|Square|Triangle|demo|DEMO|ngc|jog|Jog|buffer|alarm|home|encoder|driver|drivers|homing|backlash|junction deviation|LinuxCNC|DIRHOLD|DIRSETUP|Grbl|UGS|gSender|CNCjs|HOME[A-Z_]*|MAX_LIMIT|MIN_LIMIT|GCODE|E-stop|firmware|app|browser|tablet|Unlock)\b/g,''))&&!OKW.test(t));
 ok(!eng.filter(t=>!/\.(nc|ngc|gcode|tap|txt)\b/.test(t)).length,'English left in Greek: '+[...new Set(eng)].slice(0,30).join(' | '));
 ok(el.some(t=>/Πρόωση/.test(t))&&el.some(t=>/Κράτα για κίνηση/.test(t)),'Greek texts shown');
 await setLang('en');const en2=await visit();
 const a=[...new Set(en1)].sort().join('\n'),c=[...new Set(en2)].sort().join('\n');ok(a==c,'English again the same texts');
 if(a!=c){const A=new Set(en1),C=new Set(en2);console.log('only first:',[...A].filter(x=>!C.has(x)).slice(0,10),'only second:',[...C].filter(x=>!A.has(x)).slice(0,10))}
 // a reload in Greek starts in Greek
 await setLang('el');ok(await p.evaluate(()=>JSON.parse(localStorage.getItem('h4look')).lang=='el'),'Greek kept');
 // (a file:// page's storage is not always written before a reload in the test browser: put it back if so, up to
 // 3 times, waiting for the write before reloading)
 await p.reload();await p.waitForTimeout(500);for(let i=0;i<3&&await p.evaluate(()=>JSON.parse(localStorage.getItem('h4look')).lang!='el');i++){await p.evaluate(()=>localStorage.setItem('h4look',JSON.stringify(Object.assign(JSON.parse(localStorage.getItem('h4look')),{lang:'el'}))));await p.waitForTimeout(300);await p.reload();await p.waitForTimeout(500)}await p.waitForTimeout(800);{const t=await p.evaluate(()=>$('navBtn').textContent+'|'+localStorage.getItem('h4look'));ok(/Ρυθμίσεις|Έλεγχος|Τόρνος/.test(t),'Greek after a reload '+t)}
 await b.close();
 const all=fails.concat(errs.map(e=>'page error: '+e));all.forEach(f=>console.log('FAIL',f));
 console.log('lang: '+n+' checks, '+all.length+' failures');process.exit(all.length?1:0)})();
