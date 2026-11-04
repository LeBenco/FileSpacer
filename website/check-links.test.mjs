import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {checkLinks} from './check-links.mjs';

async function fixture(t, files) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'filespacer-links-'));
  t.after(() => fs.rm(root, {recursive: true, force: true}));
  for (const [name, content] of Object.entries(files)) {
    await fs.mkdir(path.dirname(path.join(root, name)), {recursive: true});
    await fs.writeFile(path.join(root, name), content);
  }
  return root;
}

test('badge destinations, reference links and HTML images must point to tracked files', async t => {
  const files = {'README.md': '[![Docs](https://example.org/badge.svg)](docs/user-guide.md)\n\n[Guide][doc]\n\n[doc]: docs/en/user-guide.md\n\n<img src="private.png">',
    'docs/en/user-guide.md': '# Guide', 'private.png': 'untracked'};
  const root = await fixture(t, files);
  const result = await checkLinks(root, ['README.md', 'docs/en/user-guide.md']);
  assert.deepEqual(result.failures.map(item => item.href), ['docs/user-guide.md', 'private.png']);
});

test('localized sections, HTML IDs, encoded fragments and duplicate headings resolve exactly', async t => {
  const files = {'README.md': '[ok](docs/fr/guide.md#disposition-des-ic%C3%B4nes) [duplicate](docs/fr/guide.md#suite-1) [explicit](docs/fr/guide.md#stable) [bad](docs/fr/guide.md#icon-layout)',
    'docs/fr/guide.md': '# Guide\n\n## Disposition des icônes\n\n<a id="stable"></a>\n\n## Suite\n\n## Suite'};
  const root = await fixture(t, files);
  const result = await checkLinks(root, Object.keys(files));
  assert.equal(result.failures.length, 1);
  assert.equal(result.failures[0].href, 'docs/fr/guide.md#icon-layout');
});

test('repository URLs and filename casing are checked; code examples and remote URLs are ignored', async t => {
  const files = {'README.md': '[ok](https://github.com/LeBenco/FileSpacer/blob/main/docs/guide.md#guide)\n\n[bad case](docs/Guide.md)\n\n[remote](https://example.org/elsewhere)\n\n```md\n[example](missing.md)\n```',
    'docs/guide.md': '# Guide'};
  const root = await fixture(t, files);
  const result = await checkLinks(root, Object.keys(files));
  assert.equal(result.failures.length, 1);
  assert.equal(result.failures[0].target, 'docs/Guide.md');
});
