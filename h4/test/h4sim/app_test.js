// app_test.js - the app's real page (android/app/src/main/assets/h4/index.html) in a real browser, connected to the
// whole H4 firmware running on the PC (test/h4sim), used through its buttons as the operator uses it:
//   machine settings saved on the page (the H4 restarts and keeps them), Home All with simulated switches, MDI,
//   a program uploaded with the page's form and started with its Start button, Pause / Resume, the tablet's WiFi
//   lost during a program (the H4 pauses, Resume goes on), STOP.
// Every position is checked on the simulated motors (counted from the STEP / DIR pins), not only on the page.
// Usage: node app_test.js BUILD_DIR [PORT]     (BUILD_DIR made by build.py; needs node with playwright)
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
const { chromium } = require('playwright');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const BUILD = path.resolve(process.argv[2] || '/tmp/h4sim-test');
const PORT = +(process.argv[3] || 18380);
const B = `http://127.0.0.1:${PORT}`, C = `http://127.0.0.1:${PORT + 1}`;
const PAGE = path.resolve(__dirname, '../../../android/app/src/main/assets/h4/index.html');
const H4INO = fs.readFileSync(path.resolve(__dirname, '../../h4.ino'), 'utf8');
const def = n => { const m = H4INO.match(new RegExp('^\\s*(?:long|int|bool|float)\\s+' + n + '\\s*=\\s*([^;]+);', 'm')); return m[1].trim() === 'true' ? 1 : m[1].trim() === 'false' ? 0 : +eval(m[1]); };
const SPM = { X: def('MOTOR_STEPS_X') / (def('SCREW_X_DU') / 10000), Z: def('MOTOR_STEPS_Z') / (def('SCREW_Z_DU') / 10000) };
const SIGN = { X: def('INVERT_X') ? -1 : 1, Z: def('INVERT_Z') ? -1 : 1 };
const BL = { X: Math.round(def('BACKLASH_DU_X') / 10000 * SPM.X), Z: Math.round(def('BACKLASH_DU_Z') / 10000 * SPM.Z) };

let n = 0, fails = 0;
const ok = (c, w) => { n++; if (!c) fails++; console.log((c ? 'ok  : ' : 'FAIL: ') + w); };
const sleep = ms => new Promise(r => setTimeout(r, ms));
const status = async () => (await fetch(B + '/status')).json();
const sim = async () => (await fetch(C + '/state')).json();
const ctl = (p, q) => fetch(C + p + '?' + new URLSearchParams(q), { method: 'POST' });

async function startSim() {
  fs.rmSync(path.join(BUILD, 'data'), { recursive: true, force: true });
  const p = spawn('sh', [path.join(__dirname, 'run.sh'), BUILD, String(PORT)], { detached: true, stdio: 'ignore' });
  for (let i = 0; i < 100; i++) { try { await status(); break; } catch (e) { await sleep(100); } }
  return p;
}
// waits as the operator does: until the H4 shows Idle, not ON, nothing running, twice in a row
async function waitIdle(timeoutMs = 60000) {
  const end = Date.now() + timeoutMs; let quiet = 0, s;
  await sleep(500);
  while (Date.now() < end) {
    try { s = await status(); quiet = s.state == 'Idle' && !s.on && !s.running && !s.homing ? quiet + 1 : 0; } catch (e) { quiet = 0; }
    if (quiet >= 2) return s;
    await sleep(300);
  }
  return s;
}

(async () => {
  const proc = await startSim();
  const browser = await chromium.launch();
  try {
    const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
    const errs = [];
    page.on('pageerror', e => errs.push(String(e)));
    page.on('dialog', d => d.type() == 'prompt' ? d.accept(d.defaultValue()) : d.accept());  // the operator says yes
    // the page is the app's own, served for the H4's address as the app does; everything else goes to the H4
    await page.route(u => new URL(u).pathname == '/' || new URL(u).pathname == '/index.html',
      r => r.fulfill({ status: 200, contentType: 'text/html; charset=utf-8', body: fs.readFileSync(PAGE) }));
    await page.goto(B + '/');
    await sleep(2500);
    ok(/Idle/i.test(await page.textContent('#cState')), 'the page connects to the H4 on the PC: "' + (await page.textContent('#cState')).trim() + '"');
    ok(!(await page.isVisible('#demoTag')), 'not the demo: the real firmware answers');

    // 1. machine settings on the page: home switches X and Z, as the author's machine (switch X at -20 from the
    //    centre, mill-style Z with its switch at +10), no spindle pause (router). The H4 restarts with them.
    await page.evaluate(() => tab('settings'));
    await page.waitForFunction(() => cfg && document.getElementById('c_hxUse'), null, { timeout: 10000 });
    await page.evaluate(() => {
      const set = (k, v) => { const el = document.getElementById('c_' + k); if (el.type == 'checkbox') el.checked = !!v; else el.value = v; };
      set('hxUse', 1); set('hxOff', -20); set('hxHome', 0); set('hzUse', 1); set('hzOff', 10); set('hzHome', 0);
      set('hReq', 1); set('gcPause', 0);
    });
    const bootBefore = (await status()).boot;
    await page.evaluate(() => saveCfg());
    let s;
    for (let i = 0; i < 100; i++) { await sleep(200); try { s = await status(); if (s.boot != bootBefore || i > 30) break; } catch (e) {} }
    await sleep(1500);
    const cfgNow = await (await fetch(B + '/config')).json();
    const v = k => (cfgNow.items.find(i => i.k == k) || {}).v;
    ok(v('hxUse') == 1 && v('hzUse') == 1 && Math.abs(v('hxOff') + 20) < 1e-6 && Math.abs(v('hzOff') - 10) < 1e-6,
      'settings saved from the page, the H4 restarted with them (hxUse ' + v('hxUse') + ', hxOff ' + v('hxOff') + ', hzOff ' + v('hzOff') + ')');

    // 2. Home All from the page with the simulated switches: X switch 20 mm toward minus, Z switch 5 mm toward plus
    //    (motor steps: X runs inverted, so its switch is at +20 mm x steps/mm on the motor)
    const xSw = Math.round(20 * SPM.X), zSw = Math.round(5 * SPM.Z);
    await ctl('/switch', { pin: 12, motor: 'X', op: SIGN.X < 0 ? 'ge' : 'le', steps: SIGN.X < 0 ? xSw : -xSw, pressed: 0 });
    await ctl('/switch', { pin: 13, motor: 'Z', op: SIGN.Z > 0 ? 'ge' : 'le', steps: SIGN.Z > 0 ? zSw : -zSw, pressed: 0 });
    await page.evaluate(() => tab('control'));
    await page.evaluate(() => homeAll());
    s = await waitIdle(90000);
    const m = (await sim()).motors;
    // where the switch is (motor steps) + (HOME - HOME_OFFSET): X switch = machine -20, HOME 0: 20 mm toward plus;
    // Z switch = machine +10, HOME 0: 10 mm toward minus (each within its backlash)
    const xSwM = SIGN.X < 0 ? xSw : -xSw, zSwM = SIGN.Z > 0 ? zSw : -zSw;
    const xHome = xSwM + SIGN.X * 20 * SPM.X, zHome = zSwM + SIGN.Z * -10 * SPM.Z;
    ok(s && s.homed && s.homed[0] && s.homed[1], 'Home All from the page: X and Z homed (' + JSON.stringify(s && s.homed) + ')');
    ok(Math.abs(m.X - xHome) <= BL.X + 3 && Math.abs(m.Z - zHome) <= BL.Z + 3,
      'after Home All the motors are where the switches put them: X ' + m.X + ' (about ' + xHome + '), Z ' + m.Z + ' (about ' + zHome + ')');

    // 3. MDI on the page (the Program screen)
    await page.evaluate(() => tab('program'));
    await sleep(300);
    const s0 = await status();
    await page.fill('#mdi', 'G0 X10 Z-8');
    await page.click('#bMdi');
    s = await waitIdle();
    const m1 = (await sim()).motors;
    ok(Math.abs(s.x - 10) < 1e-3 && Math.abs(s.z + 8) < 1e-3, 'MDI from the page: the H4 at X ' + s.x + ' Z ' + s.z);
    // the motors moved as far as the H4 says the tool did (the G-code zero is where the H4 started: Z work = machine - 5)
    ok(Math.abs((m1.X - m.X) - SIGN.X * (s.x - s0.x) * SPM.X) <= BL.X + 1 && Math.abs((m1.Z - m.Z) - SIGN.Z * (s.z - s0.z) * SPM.Z) <= BL.Z + 1,
      'and the motors moved as far as the H4 says (X ' + (m1.X - m.X) + ' steps for ' + (s.x - s0.x).toFixed(3) + ' mm, Z ' + (m1.Z - m.Z) + ' for ' + (s.z - s0.z).toFixed(3) + ' mm)');

    // 4. upload with the page's form, choose it, Start
    const prog = 'G21 G90\nG0 X5 Z0\nG1 Z-2 F600\nG1 X1\nG1 Z0\nG0 X3\nM2\n';
    await page.setInputFiles('#file', { name: 'square.ngc', mimeType: 'text/plain', buffer: Buffer.from(prog) });
    await page.evaluate(() => document.querySelector('#up input[type=submit]').click());
    await page.waitForFunction(() => [...document.querySelectorAll('#files input[name=sel]')].some(i => i.value == 'square.ngc'), null, { timeout: 10000 });
    await page.check('#files input[value="square.ngc"]');
    await page.click('#bStart');
    s = await waitIdle();
    ok(Math.abs(s.x - 3) < 1e-3 && Math.abs(s.z) < 1e-3, 'upload + Start from the page: the program ended at X ' + s.x + ' Z ' + s.z);

    // 5. Pause / Resume buttons
    await page.setInputFiles('#file', { name: 'slow.ngc', mimeType: 'text/plain', buffer: Buffer.from('G21 G90\nG1 X9 F120\nM2\n') });
    await page.evaluate(() => document.querySelector('#up input[type=submit]').click());
    await page.waitForFunction(() => [...document.querySelectorAll('#files input[name=sel]')].some(i => i.value == 'slow.ngc'), null, { timeout: 10000 });
    await page.check('#files input[value="slow.ngc"]');
    await page.click('#bStart');
    await sleep(1500);
    await page.click('button.pause');
    await sleep(800);
    const a = (await sim()).motors.X; await sleep(1000); const b = (await sim()).motors.X;
    ok((await status()).state == 'Hold' && a == b, 'Pause on the page: the H4 holds, the X motor stands still (' + a + ')');
    await page.click('button:text-is("Resume")');
    s = await waitIdle();
    ok(Math.abs(s.x - 9) < 1e-3, 'Resume on the page: the program ends at X ' + s.x);

    // 6. the tablet's WiFi lost during a program: the page cannot reach the H4 for 5 s
    await page.setInputFiles('#file', { name: 'away.ngc', mimeType: 'text/plain', buffer: Buffer.from('G21 G90\nG1 X5 F60\nM2\n') });
    await page.evaluate(() => document.querySelector('#up input[type=submit]').click());
    await page.waitForFunction(() => [...document.querySelectorAll('#files input[name=sel]')].some(i => i.value == 'away.ngc'), null, { timeout: 10000 });
    await page.check('#files input[value="away.ngc"]');
    await page.click('#bStart');
    await sleep(1500);
    await page.route('**/status', r => r.abort());
    await sleep(5000);
    ok(/NO CONNECTION/i.test(await page.textContent('#cState')), 'WiFi lost: the page shows "' + (await page.textContent('#cState')).trim() + '"');
    s = await status();
    ok(s.state == 'Hold' && /connection was lost/.test(s.text), 'the H4 paused by itself: "' + s.text.slice(0, 50) + '"');
    await page.unroute('**/status');
    await sleep(1500);
    ok(/Hold/i.test(await page.textContent('#cState')), 'WiFi back: the page shows the pause again');
    await page.click('button:text-is("Resume")');
    s = await waitIdle();
    ok(Math.abs(s.x - 5) < 1e-3, 'Resume: the program ends at X ' + s.x);

    // 7. STOP from the page's header in the middle of a move
    await page.fill('#mdi', 'G1 X20 F120');
    await page.click('#bMdi');
    await sleep(2000);
    await page.click('button.hstop');
    s = await waitIdle();
    const m2 = (await sim()).motors;
    ok(s.x > 5.5 && s.x < 19.5 && Math.abs((m2.X - m1.X) - SIGN.X * (s.x - 10) * SPM.X) <= BL.X + 2,
      'STOP on the page: stopped at X ' + s.x.toFixed(4) + ', the X motor agrees');
    ok(errs.length == 0, 'no error in the page' + (errs.length ? ': ' + errs.slice(0, 3).join(' | ') : ''));
  } catch (e) {
    ok(false, 'exception: ' + e.stack);
  } finally {
    await browser.close();
    try { process.kill(-proc.pid, 'SIGTERM'); } catch (e) {}
  }
  console.log('h4sim app: ' + n + ' checks, ' + fails + ' failures');
  process.exit(fails ? 1 : 0);
})();
