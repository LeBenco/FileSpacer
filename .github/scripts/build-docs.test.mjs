// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import vm from 'node:vm';
import {localUrl, offlineSearch, installerInclude, sourceDigest} from './build-docs.mjs';

test('source fingerprints survive CRLF transfer and ignore build caches while tracking runtime assets', async t => {
  const directory = await fs.mkdtemp(path.join(os.tmpdir(), 'filespacer-docs-input-'));
  t.after(() => {
    assert.equal(path.dirname(directory), os.tmpdir());
    return fs.rm(directory, {recursive: true, force: true});
  });
  for (const file of ['README.md', 'CHANGELOG.md', 'THIRD_PARTY_NOTICES.md', 'docs/user-guide.md', 'docs/privacy-policy.md', 'src/res/FileSpacer.svg', 'website/assets/js/vendor/search.js', '.github/scripts/Gemfile.lock', 'docs/images/image.png']) {
    await fs.mkdir(path.dirname(path.join(directory, file)), {recursive: true});
    await fs.writeFile(path.join(directory, file), 'first\nsecond\n');
  }
  const original = await sourceDigest(directory);
  await fs.writeFile(path.join(directory, 'README.md'), 'first\r\nsecond\r\n');
  await fs.mkdir(path.join(directory, '.github/scripts/.sass-cache'), {recursive: true});
  await fs.writeFile(path.join(directory, '.github/scripts/.sass-cache/generated'), 'cache');
  assert.equal(await sourceDigest(directory), original);
  await fs.writeFile(path.join(directory, 'website/assets/js/vendor/search.js'), 'changed runtime');
  assert.notEqual(await sourceDigest(directory), original);
});

test('offline links preserve language, section and resources at any install path', () => {
  const convert = (url, file = 'fr/index.html') => localUrl(url, file, '/FileSpacer', 'https://lebenco.github.io');
  assert.equal(convert('/FileSpacer/fr/'), 'index.html');
  assert.equal(convert('/FileSpacer/'), '../index.html');
  assert.equal(convert('/FileSpacer/de/#user-guide'), '../de/index.html#user-guide');
  assert.equal(convert('https://lebenco.github.io/FileSpacer/fr/#user-guide'), 'index.html#user-guide');
  assert.equal(convert('/FileSpacer/fr/assets/framing.css?v=1'), 'assets/framing.css?v=1');
  assert.equal(convert('/FileSpacer/fr/assets/icon.png', 'fr/assets/theme.css'), 'icon.png');
  assert.equal(convert('#user-guide'), '#user-guide');
  assert.equal(convert('https://github.com/LeBenco/FileSpacer'), 'https://github.com/LeBenco/FileSpacer');
  assert.equal(convert('/FileSpacer-other/'), '/FileSpacer-other/');
});

test('offline search retains the theme indexer and loads data without HTTP', () => {
  const script = `function initSearch() {
  var request = new XMLHttpRequest();
  request.onload = function(){
    if (request.status >= 200 && request.status < 400) {
      var docs = JSON.parse(request.responseText);
      lunr.tokenizer.separator = /[\\s\\-/]+/;
      var index = {sentinel: docs[0].title};
      searchLoaded(index, docs);
    }
  };
  request.send();
}
function searchLoaded(index, docs) { result = [index.sentinel, docs[0].url]; }
initSearch();`;
  const offline = offlineSearch(script, {0: {title: 'Μετακίνηση <test>', url: 'index.html#user-guide'}});
  assert.doesNotMatch(offline, /XMLHttpRequest|request\.send/);
  const context = {lunr: {tokenizer: {}}};
  vm.runInNewContext(offline, context);
  assert.equal(context.result[0], 'Μετακίνηση <test>');
  assert.equal(context.result[1], 'index.html#user-guide');
  assert.throws(() => offlineSearch('function initSearch() {}', {}), /theme search loader changed/);
});

test('installer removes only the documentation files it owns', async t => {
  const directory = await fs.mkdtemp(path.join(os.tmpdir(), 'filespacer-docs-nsis-'));
  t.after(() => {
    assert.equal(path.dirname(directory), os.tmpdir());
    return fs.rm(directory, {recursive: true, force: true});
  });
  const output = path.join(directory, 'offline-docs');
  await installerInclude(output, ['index.html', 'fr/index.html', 'fr/assets/a.css']);
  const include = await fs.readFile(output + '.nsh', 'utf8');
  assert.match(include, /File "\$\{FILESPACER_DOCS\}\\fr\\assets\\a.css"/);
  assert.match(include, /Delete "\$INSTDIR\\help\\fr\\index.html"/);
  assert.doesNotMatch(include, /\/r|\*/i);
  assert.ok(include.indexOf('RMDir "$INSTDIR\\help\\fr\\assets"') < include.indexOf('RMDir "$INSTDIR\\help\\fr"'));
  assert.ok(include.indexOf('RMDir "$INSTDIR\\help\\fr"') < include.indexOf('RMDir "$INSTDIR\\help"'));
});
