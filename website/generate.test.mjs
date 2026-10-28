import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {generate} from './generate.mjs';

const repository = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
async function fixture(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'filespacer-site-fixture-'));
  t.after(() => {
    assert.equal(path.dirname(root), os.tmpdir());
    return fs.rm(root, {recursive: true, force: true});
  });
  for (const source of ['README.md', 'docs/user-guide.md', 'docs/privacy-policy.md', 'CHANGELOG.md', 'THIRD_PARTY_NOTICES.md', 'src/res/FileSpacer.svg']) {
    await fs.mkdir(path.dirname(path.join(root, source)), {recursive: true});
    await fs.copyFile(path.join(repository, source), path.join(root, source));
  }
  await fs.cp(path.join(repository, 'docs/images'), path.join(root, 'docs/images'), {recursive: true});
  return root;
}
test('real documents keep guide nesting, local anchors, images and GitHub references', async t => {
  const output = await fs.mkdtemp(path.join(os.tmpdir(), 'filespacer-site-'));
  t.after(() => {
    assert.equal(path.dirname(output), os.tmpdir());
    return fs.rm(output, {recursive: true, force: true});
  });
  const result = await generate(repository, output);
  assert.equal(result.documents, 5);
  const html = await fs.readFile(path.join(output, 'index.html'), 'utf8');
  const nav = await fs.readFile(path.join(output, '_includes/components/site_nav.html'), 'utf8');
  assert.match(html, /href="#user-guide--moving-and-resizing"/);
  assert.match(html, /href="#user-guide--everything"/);
  assert.match(html, /href="#user-guide--nilesoft-shell"/);
  assert.match(html, /https:\/\/github\.com\/LeBenco\/FileSpacer\/blob\/main\/docs\/build\.md/);
  assert.match(html, /https:\/\/github\.com\/LeBenco\/FileSpacer\/blob\/main\/tests\/regression\/README\.md/);
  assert.doesNotMatch(html, /data-source="(?:docs\/(?:build|native-shell-view)|tests\/)/);
  assert.match(nav, /href="#user-guide--integration"[^]*href="#user-guide--windows-integration"[^]*href="#user-guide--everything"[^]*href="#user-guide--nilesoft-shell"/);
  assert.match(html, /<h4 id="user-guide--everything"/);
  const windows = html.match(/<h3 id="user-guide--managing-windows">[^]*?(?=<h3 |<\/section>)/)?.[0];
  assert.ok(windows, 'The user guide groups window controls under Managing windows');
  assert.match(windows, /<h4 id="user-guide--moving-and-resizing"/);
  assert.match(windows, /<h4 id="user-guide--locking-a-saved-layout"/);
  assert.match(html, /href="#user-guide--locking-a-saved-layout"/);
  assert.match(html, /assets\/content\/src\/res\/FileSpacer\.svg/);
  await fs.access(path.join(output, 'assets/content/src/res/FileSpacer.svg'));
});

test('broken source anchors fail generation instead of publishing broken navigation', async t => {
  const root = await fixture(t);
  await fs.appendFile(path.join(root, 'README.md'), '\n[Broken](docs/user-guide.md#not-a-real-heading)\n');
  await assert.rejects(generate(root, path.join(root, 'output')), /Missing heading/);
});

test('repeated headings keep unique IDs and links to numbered occurrences', async t => {
  const root = await fixture(t);
  await fs.appendFile(path.join(root, 'docs/user-guide.md'), '\n## Repeated topic\n\nFirst.\n\n## Repeated topic\n\nSecond.\n');
  await fs.appendFile(path.join(root, 'README.md'), '\n[Second topic](docs/user-guide.md#repeated-topic-1)\n');
  await generate(root, path.join(root, 'output'));
  const html = await fs.readFile(path.join(root, 'output/index.html'), 'utf8');
  assert.match(html, /id="user-guide--repeated-topic"/);
  assert.match(html, /id="user-guide--repeated-topic-1"/);
  assert.match(html, /href="#user-guide--repeated-topic-1"/);
});

async function translatedFixture(t) {
  const root = await fixture(t);
  const target = path.join(root, 'website/translations/fr-FR');
  await fs.mkdir(path.dirname(target), {recursive: true});
  await fs.cp(path.join(repository, 'website/translations/fr-FR'), target, {recursive: true});
  return {root, target};
}

test('all available translations keep English section IDs, localized navigation and their own search configuration', async t => {
  const output = await fs.mkdtemp(path.join(os.tmpdir(), 'filespacer-languages-'));
  t.after(() => {
    assert.equal(path.dirname(output), os.tmpdir());
    return fs.rm(output, {recursive: true, force: true});
  });
  const result = await generate(repository, output);
  const applicationLanguages = (await fs.readdir(path.join(repository, 'src')))
    .filter(name => /^resource\.[a-z]{2}\.rc$/.test(name))
    .map(name => name.split('.')[1]).sort();
  assert.deepEqual(result.languages.map(language => language.code.split('-')[0]).sort(), applicationLanguages);
  const englishHtml = await fs.readFile(path.join(output, 'index.html'), 'utf8');
  const englishIds = [...englishHtml.matchAll(/\bid="([^"]+)"/g)].map(match => match[1]);
  for (const language of result.languages) {
    const directory = path.join(output, language.source);
    const html = await fs.readFile(path.join(directory, 'index.html'), 'utf8');
    const nav = await fs.readFile(path.join(directory, '_includes/components/site_nav.html'), 'utf8');
    const config = await fs.readFile(path.join(directory, '_config.yml'), 'utf8');
    assert.deepEqual([...html.matchAll(/\bid="([^"]+)"/g)].map(match => match[1]), englishIds);
    assert.equal((nav.match(/data-language-href=/g) || []).length, result.languages.length);
    assert.ok(nav.indexOf('language-picker') < nav.indexOf('<ul class="nav-list">'));
    assert.ok(nav.includes('hreflang="' + language.code + '"'));
    assert.ok(config.includes('lang: ' + language.code));
    assert.ok(config.includes('baseurl: /FileSpacer' + (language.path ? '/' + language.path : '')));
    assert.equal(language.images, result.images);
  }
  const localizedImage = await fs.readFile(path.join(output, '_locales/fr-FR/assets/content/docs/images/overview.png'));
  assert.deepEqual(localizedImage, await fs.readFile(path.join(repository, 'docs/images/overview-fr.png')));
});

test('a translation with a missing document is rejected rather than partially published', async t => {
  const {root, target} = await translatedFixture(t);
  await fs.unlink(path.join(target, 'docs/privacy-policy.md'));
  await assert.rejects(generate(root, path.join(root, 'output')), /Incomplete translation fr-FR: missing docs\/privacy-policy\.md/);
});

test('missing localized media and mismatched translation headings fail the build', async t => {
  const {root, target} = await translatedFixture(t);
  const image = path.join(root, 'docs/images/overview-fr.png');
  const bytes = await fs.readFile(image);
  await fs.unlink(image);
  await assert.rejects(generate(root, path.join(root, 'output')), /Missing localized media \(fr-FR\)/);
  await fs.writeFile(image, bytes);
  await fs.appendFile(path.join(target, 'docs/user-guide.md'), '\n## Une rubrique supplémentaire\n');
  await assert.rejects(generate(root, path.join(root, 'output')), /Translation heading (?:structure|count) differs from English/);
});
