// app_crawl.js - every screen of the app's page (demo mode, no H4 needed), in English and Greek, at a tablet's,
// a portrait tablet's and a phone's size: every visible button is pressed once (a dialog is answered, a sheet that
// opens is closed again); any JavaScript error of the page fails the test. Buttons that reload the page (restart,
// firmware, delete, reset, WiFi) are left out. Usage: NODE_PATH=<dir with playwright> node app_crawl.js
// Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
const path = require('path');
const PAGE = 'file://' + path.resolve(__dirname, '../../android/app/src/main/assets/h4/index.html') + '?demo';
(async () => {
  const b = await chromium.launch();
  const errs = [];
  let clicks = 0;
  for (const vp of [{ width: 1024, height: 600 }, { width: 600, height: 1024 }, { width: 400, height: 800 }]) {
    const p = await b.newPage({ viewport: vp });
    let last = '';
    p.on('pageerror', e => errs.push(vp.width + 'x' + vp.height + ', after ' + last + ': ' + e.message));
    p.on('dialog', d => d.type() == 'prompt' ? d.accept('5') : d.accept());
    await p.goto(PAGE);
    await p.waitForTimeout(1200);
    for (const lang of ['en', 'el']) {
      await p.evaluate(l => { look.lang = l; }, lang);
      for (const t of await p.evaluate(() => TN)) {
        await p.evaluate(t => tab(t), t);
        await p.waitForTimeout(300);
        const n = await p.evaluate(() => document.querySelectorAll('button').length);
        for (let i = 0; i < n; i++) {
          const what = await p.evaluate(i => {
            const x = document.querySelectorAll('button')[i];
            if (!x) return '';
            const r = x.getBoundingClientRect(), txt = (x.textContent || '').toLowerCase().trim();
            if (!r.width || !r.height || x.disabled) return '';
            if (/restart|επανεκκίνηση|firmware|delete|διαγραφ|reset|wifi/.test(txt)) return '';
            x.click();
            return (x.id || '') + ' "' + txt.slice(0, 30) + '"';
          }, i);
          if (what) { clicks++; last = t + ' ' + what; await p.waitForTimeout(40); }
          await p.evaluate(() => { if (typeof sheetClose == 'function') sheetClose(); });
          if (!p.url().startsWith('file:')) { await p.goto(PAGE); await p.waitForTimeout(800); }
        }
      }
    }
    await p.close();
  }
  await b.close();
  for (const e of [...new Set(errs)]) console.log('FAIL: ' + e);
  console.log('app crawl: ' + clicks + ' buttons pressed, ' + errs.length + ' page errors');
  process.exit(errs.length ? 1 : 0);
})();
