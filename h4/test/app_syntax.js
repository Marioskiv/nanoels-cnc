// app_syntax.js - the app's page (android/app/src/main/assets/h4/index.html) and the H4's own pages must parse as
// ECMAScript 2017: the tablets are Android 7 (WebView updates are not guaranteed there); async / await is the newest
// syntax used. Newer syntax (?? and ?. are ES2020) would leave the whole screen blank on an older WebView.
// Usage: NODE_PATH=<dir with acorn> node app_syntax.js       (exit 1 on a problem)
// Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: GPL-3.0-or-later
const fs = require('fs'), path = require('path'), acorn = require('acorn');
const ROOT = path.resolve(__dirname, '..', '..');
const files = ['android/app/src/main/assets/h4/index.html', 'h4/sender.html'];
let bad = 0, scripts = 0;
for (const f of files) {
  const p = path.join(ROOT, f);
  if (!fs.existsSync(p)) continue;
  const html = fs.readFileSync(p, 'utf8');
  const re = /<script(?![^>]*\bsrc=)[^>]*>([\s\S]*?)<\/script>/g;
  let m;
  while ((m = re.exec(html))) {
    scripts++;
    const before = html.slice(0, m.index + m[0].indexOf('>') + 1).split('\n').length - 1;
    try {
      acorn.parse(m[1], { ecmaVersion: 2017, sourceType: 'script' });
    } catch (e) {
      bad++;
      console.log('FAIL: ' + f + ' line ' + (before + e.loc.line) + ': ' + e.message.replace(/ \(\d+:\d+\)$/, '') + ' (not ES2017)');
    }
  }
}
console.log('app syntax: ' + scripts + ' scripts checked as ES2017, ' + bad + ' failures');
process.exit(bad ? 1 : 0);
