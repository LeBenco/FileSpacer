import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import {fileURLToPath} from 'node:url';
import {Lexer, walkTokens} from 'marked';
import {prepareHeadingAnchors} from './generate.mjs';
import {normalizeDocumentation} from './normalize-docs.mjs';

async function fixture(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'filespacer-normalize-'));
  t.after(() => fs.rm(root, {recursive: true, force: true}));
  const entries = {
    'README.md': '# FileSpacer\r\n\r\n[Menu](docs/fr/nilesoft-shell.md#apparence)\r\n',
    'CHANGELOG.md': '# Changelog\n', 'THIRD_PARTY_NOTICES.md': '# Credits\n', 'BUILD.md': '# Build\n',
    'docs/en/user-guide.md': '# User guide\n\n## View\n\nSquare corners are recommended for title-bar and toolbar menus.\n\n**Main menu (…)**\n',
    'docs/fr/user-guide.md': '# Guide\n\n## Affichage\n\nDes coins carrés sont recommandés pour les menus de la barre de titre et de la barre d’outils.\n\n![Menu](images/menu-fr.png)\n\n**Menu principal (…)**\n',
    'docs/en/nilesoft-shell.md': '# Nilesoft\n\n## Recommended menu appearance\n',
    'docs/fr/nilesoft-shell.md': '# Nilesoft\n\n## Apparence\n',
    'docs/en/images/menu.png': 'English image', 'docs/fr/images/menu-fr.png': 'French image',
    'website/example.mjs': 'const image = /docs\\/fr\\/images\\/menu-fr\\.png/;\n',
  };
  for (const directory of ['website', '.github', 'tests']) await fs.mkdir(path.join(root, directory), {recursive: true});
  for (const [file, content] of Object.entries(entries)) {
    await fs.mkdir(path.dirname(path.join(root, file)), {recursive: true});
    await fs.writeFile(path.join(root, file), content);
  }
  return root;
}

test('the scripted migration shares English anchors and image names, updates escaped paths and is idempotent', async t => {
  const root = await fixture(t);
  const result = await normalizeDocumentation(root);
  assert.deepEqual(result.renamed, {'docs/fr/images/menu-fr.png': 'docs/fr/images/menu.png'});
  assert.equal(await fs.readFile(path.join(root, 'docs/fr/images/menu.png'), 'utf8'), 'French image');
  await assert.rejects(fs.access(path.join(root, 'docs/fr/images/menu-fr.png')));
  const french = await fs.readFile(path.join(root, 'docs/fr/user-guide.md'), 'utf8');
  assert.match(french, /<a id="view"><\/a>\n\n## Affichage/);
  assert.match(french, /\[Des coins carrés\]\(nilesoft-shell\.md#recommended-menu-appearance\)/);
  assert.ok(french.includes('images/menu.png'));
  assert.ok(french.includes('**Menu principal**'));
  assert.ok(!french.includes('(…)'));
  const readme = await fs.readFile(path.join(root, 'README.md'), 'utf8');
  assert.ok(readme.includes('nilesoft-shell.md#recommended-menu-appearance'));
  assert.equal(readme.replaceAll('\r\n', '').includes('\n'), false, 'Existing CRLF must be preserved');
  assert.equal(await fs.readFile(path.join(root, 'website/example.mjs'), 'utf8'), 'const image = /docs\\/fr\\/images\\/menu\\.png/;\n');
  assert.deepEqual(await normalizeDocumentation(root), {changed: [], renamed: {}});
});

test('filename collisions stop the migration before any document is written', async t => {
  const root = await fixture(t);
  const readme = await fs.readFile(path.join(root, 'README.md'));
  await fs.writeFile(path.join(root, 'docs/fr/images/menu.png'), 'Do not overwrite');
  await assert.rejects(normalizeDocumentation(root), /Destination already exists/);
  assert.deepEqual(await fs.readFile(path.join(root, 'README.md')), readme);
  assert.equal(await fs.readFile(path.join(root, 'docs/fr/images/menu.png'), 'utf8'), 'Do not overwrite');
});

test('all real language directories use explicit English heading anchors and filenames without language suffixes', async () => {
  const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
  const entries = (await fs.readdir(path.join(root, 'docs'), {recursive: true})).map(file => file.split(path.sep).join('/'));
  const languages = [...new Set(entries.filter(file => /^[a-z]{2}\//.test(file)).map(file => file.slice(0, 2)))];
  const suffix = new RegExp(`[-_.](?:${languages.join('|')})(?:[-_][A-Z]{2})?\\.[^.]+$`, 'i');
  for (const file of entries.filter(file => /^[a-z]{2}\//.test(file))) {
    assert(!suffix.test(path.posix.basename(file)), file);
    if (!file.endsWith('.md')) continue;
    const tokens = prepareHeadingAnchors(Lexer.lex(await fs.readFile(path.join(root, 'docs', file), 'utf8')));
    walkTokens(tokens, token => {
      if (token.type === 'heading') assert.match(token.docAnchor || '', /^[a-z0-9_-]+$/, `${file}: explicit English anchor`);
    });
  }
  for (const language of languages) {
    const guide = await fs.readFile(path.join(root, `docs/${language}/user-guide.md`), 'utf8');
    assert.ok(guide.includes('(nilesoft-shell.md#recommended-menu-appearance)'), language);
    assert.doesNotMatch(guide, /\*\*[^*\r\n]+\s*\(…\)\*\*/);
  }
});
