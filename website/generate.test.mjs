import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {generate} from './generate.mjs';
import lunr from 'lunr';

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
  const result = await generate(repository, output, {onWarning: () => {}});
  assert.equal(result.documents, 4);
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
  assert.doesNotMatch(nav, /href="#privacy-policy"/);
  assert.match(nav, /href="#user-guide--privacy"/);
  for (const [alias, target] of [
    ['privacy-policy', 'privacy'], ['privacy-policy--local-data', 'local-data'],
    ['privacy-policy--resetting-preferences', 'local-data'],
    ['privacy-policy--update-checks', 'settings-and-updates'],
    ['privacy-policy--windows-and-external-applications', 'privacy'],
  ]) assert.ok(html.includes(`id="${alias}" data-alias-for="user-guide--${target}"`));
  const screenshots = [...html.matchAll(/<a class="screenshot-link"[^>]+><img\b[^>]+><\/a>/g)];
  assert.equal(screenshots.length, [...html.matchAll(/<img\b[^>]*src="[^"]+\.(?:png|gif)\?v=[a-f0-9]{12}"/g)].length);
  assert.ok(screenshots.length > 0);
  assert.ok(screenshots.every(([link]) => !link.includes('.svg')));
  assert.match(html, /screenshot-link" href="assets\/content\/docs\/images\/spatial-windows\.png\?v=[a-f0-9]{12}"/);
  await fs.access(path.join(output, 'assets/content/src/res/FileSpacer.svg'));
});

test('replacing an image changes both its thumbnail and popup URL while unchanged images keep their cache key', async t => {
  const root = await fixture(t);
  const output = path.join(root, 'output');
  await generate(root, output);
  const before = await fs.readFile(path.join(output, 'index.html'), 'utf8');
  const imageUrl = (html, name) => html.match(new RegExp(`href="(assets/content/docs/images/${name}\\.png\\?v=[a-f0-9]{12})"`))[1];
  await fs.copyFile(path.join(root, 'docs/images/selected-name.png'), path.join(root, 'docs/images/spatial-windows.png'));
  await generate(root, output);
  const after = await fs.readFile(path.join(output, 'index.html'), 'utf8');
  const updated = imageUrl(after, 'spatial-windows');
  assert.notEqual(updated, imageUrl(before, 'spatial-windows'));
  assert.ok(after.includes(`src="${updated}"`));
  assert.equal(imageUrl(after, 'window-menu'), imageUrl(before, 'window-menu'));
});

test('the header adds no download button while preserving download links in Markdown', async t => {
  const root = await fixture(t);
  await fs.appendFile(path.join(root, 'README.md'), '\n[Download](https://github.com/LeBenco/FileSpacer/releases/latest)\n');
  await generate(root, path.join(root, 'output'));
  const config = await fs.readFile(path.join(root, 'output/_config.yml'), 'utf8');
  const header = config.match(/^aux_links:\n([^]*?)(?=^[^\s])/m)[1];
  assert.doesNotMatch(header, /releases\/latest|Download/);
  assert.match(header, /https:\/\/github.com\/LeBenco\/FileSpacer/);
  const html = await fs.readFile(path.join(root, 'output/index.html'), 'utf8');
  assert.match(html, /href="https:\/\/github.com\/LeBenco\/FileSpacer\/releases\/latest">Download<\/a>/);
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

test('explicit HTML anchors and raw HTML links are resolved without cross-document collisions', async t => {
  const root = await fixture(t);
  await fs.appendFile(path.join(root, 'README.md'), '\n<a id="custom-anchor"></a>\n[Jump](#custom-anchor)\n<a href="docs/user-guide.md#custom-anchor">Guide</a>\n<div data-id="not-an-anchor">Data</div>\n');
  await fs.appendFile(path.join(root, 'docs/user-guide.md'), "\n<div id='custom-anchor'>Target</div>\n");
  const output = path.join(root, 'output');
  await generate(root, output);
  const html = await fs.readFile(path.join(output, 'index.html'), 'utf8');
  assert.match(html, /id="overview--custom-anchor"/);
  assert.match(html, /href="#overview--custom-anchor"/);
  assert.match(html, /id='user-guide--custom-anchor'/);
  assert.match(html, /href="#user-guide--custom-anchor"/);
  assert.match(html, /data-id="not-an-anchor"/);
  await fs.appendFile(path.join(root, 'README.md'), '\n<a id="custom-anchor"></a>\n');
  await assert.rejects(generate(root, output), /Duplicate source anchor/);
});

async function translatedFixture(t) {
  const root = await fixture(t);
  const target = path.join(root, 'website/translations/fr-FR');
  await fs.mkdir(path.dirname(target), {recursive: true});
  await fs.cp(path.join(repository, 'website/translations/fr-FR'), target, {recursive: true});
  // Keep policy fixtures independent of whether the live translations lag
  // behind the live English documentation at the time the tests run.
  for (const [source, english, french] of [
    ['README.md', '# FileSpacer\n\n<img src="docs/images/spatial-windows.png" alt="Folders">\n\n## Features\n\nEnglish.', '# FileSpacer\n\n<img src="docs/images/spatial-windows.png" alt="Dossiers">\n\n## Fonctionnalités\n\nFrançais.'],
    ['docs/user-guide.md', '# User guide\n\n## Privacy\n\nPolicy.', '# Guide\n\n## Confidentialité\n\nPolitique.'],
    ['CHANGELOG.md', '# Changelog\n\n## 1.3.1\n\nReleased.', '# Historique\n\n## 1.3.1\n\nPublié.'],
    ['THIRD_PARTY_NOTICES.md', '# Third-party notices\n\n## Components\n\nCredits.', '# Composants tiers\n\n## Composants\n\nCrédits.'],
  ]) {
    await fs.writeFile(path.join(root, source), english + '\n');
    await fs.writeFile(path.join(target, source), french + '\n');
  }
  await generate(root, path.join(root, 'initial'), {recordTranslations: ['fr-FR']});
  return {root, target};
}

test('technical references use an available translation and preserve its section link', async t => {
  const {root, target} = await translatedFixture(t);
  const output = path.join(root, 'output');
  await fs.appendFile(path.join(root, 'README.md'), '\n[Build guide](docs/build.md#releases).\n');
  await fs.appendFile(path.join(target, 'README.md'), '\n[Guide de compilation](docs/build.md#releases).\n');
  await generate(root, output, {recordTranslations: ['fr-FR']});
  const page = path.join(output, '_locales/fr-FR/index.html');
  const canonical = 'https://github.com/LeBenco/FileSpacer/blob/main/docs/build.md#releases';
  assert.ok((await fs.readFile(page, 'utf8')).includes(`href="${canonical}"`));
  const technical = path.join(target, 'docs/build.md');
  await fs.writeFile(technical, '# Compilation\n\n<a id="releases"></a>\n\n## Publications\n');
  await generate(root, output);
  assert.ok((await fs.readFile(page, 'utf8')).includes('href="https://github.com/LeBenco/FileSpacer/blob/main/website/translations/fr-FR/docs/build.md#releases"'));
  assert.ok((await fs.readFile(path.join(output, 'index.html'), 'utf8')).includes(`href="${canonical}"`));
  await fs.unlink(technical);
  await generate(root, output);
  assert.ok((await fs.readFile(page, 'utf8')).includes(`href="${canonical}"`));
});

test('English text and structure updates retain translated text and historical section IDs with warnings', async t => {
  const {root} = await translatedFixture(t);
  const output = path.join(root, 'output');
  await generate(root, output);
  const translated = await fs.readFile(path.join(output, '_locales/fr-FR/index.html'), 'utf8');
  const messages = [];
  await fs.appendFile(path.join(root, 'README.md'), '\nFresh English information.\n');
  let result = await generate(root, output, {onWarning: message => messages.push(message)});
  assert.equal(result.warnings.length, 1);
  assert.equal(result.warnings[0].source, 'README.md');
  assert.equal(messages.length, 1);
  await fs.appendFile(path.join(root, 'README.md'), '\n## Newly published section\n\nNew English structure.\n');
  result = await generate(root, output, {onWarning: () => {}});
  assert.equal(result.warnings.length, 1);
  assert.match(await fs.readFile(path.join(output, 'index.html'), 'utf8'), /New English structure/);
  assert.equal(await fs.readFile(path.join(output, '_locales/fr-FR/index.html'), 'utf8'), translated);
});

test('recording synchronized sources updates only selected locales and clears stale-source warnings', async t => {
  const {root, target} = await translatedFixture(t);
  await fs.appendFile(path.join(root, 'README.md'), '\n## New section\n\nEnglish.\n');
  const output = path.join(root, 'output');
  await assert.rejects(generate(root, output, {recordTranslations: ['fr-FR']}), /Translation heading/);
  await fs.appendFile(path.join(target, 'README.md'), '\n## Nouvelle rubrique\n\nFrançais.\n');
  await generate(root, output, {recordTranslations: ['fr-FR']});
  const result = await generate(root, output);
  assert.deepEqual(result.warnings, []);
  assert.match(await fs.readFile(path.join(output, '_locales/fr-FR/index.html'), 'utf8'), /id="overview--new-section"/);
  await assert.rejects(generate(root, output, {recordTranslations: ['zz-ZZ']}), /Unknown translation locale/);
});

test('all available translations keep recorded section IDs, localized navigation and their own search configuration', async t => {
  const output = await fs.mkdtemp(path.join(os.tmpdir(), 'filespacer-languages-'));
  t.after(() => {
    assert.equal(path.dirname(output), os.tmpdir());
    return fs.rm(output, {recursive: true, force: true});
  });
  const result = await generate(repository, output, {onWarning: () => {}});
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
    if (!language.path) assert.deepEqual([...html.matchAll(/\bid="([^"]+)"/g)].map(match => match[1]), englishIds);
    else {
      const state = JSON.parse(await fs.readFile(path.join(repository, language.source.replace('_locales/', 'website/translations/'), 'source-state.json'), 'utf8'));
      for (const [, id, source, section] of html.matchAll(/<section aria-labelledby="([^"]+)" data-source="([^"]+)">([^]*?)<\/section>/g)) {
        const expected = state.sources[source].headings.map(([anchor], index) => index ? `${id}--${anchor}` : id);
        assert.deepEqual([...section.matchAll(/<h[2-6] id="([^"]+)"/g)].map(match => match[1]), expected);
      }
    }
    assert.equal((nav.match(/data-language-href=/g) || []).length, result.languages.length);
    assert.ok(nav.indexOf('language-picker') < nav.indexOf('<ul class="nav-list">'));
    assert.ok(nav.includes('hreflang="' + language.code + '"'));
    assert.ok(config.includes('lang: ' + language.code));
    const ui = JSON.parse(config.match(/^fs_ui: (.+)$/m)[1]);
    assert.equal(ui.download, undefined);
    assert.doesNotMatch(config, /\{version\}/);
    assert.match(html, /class="screenshot-link"[^>]+aria-label="[^"]+"/);
    assert.ok(config.includes('baseurl: /FileSpacer' + (language.path ? '/' + language.path : '')));
    for (const [, asset] of html.matchAll(/(?:src|href)="(assets\/content\/[^"?]+)(?:\?[^"]*)?"/g)) {
      await fs.access(path.join(directory, asset));
    }
    const reference = (language.path ? `website/translations/${language.code}/` : '') + 'docs/nilesoft-shell.md';
    assert.ok(html.includes('href="https://github.com/LeBenco/FileSpacer/blob/main/' + reference + '"'), 'Nilesoft reference must use the selected language');
    await fs.access(path.join(repository, reference));
  }
  const localizedImage = await fs.readFile(path.join(output, '_locales/fr-FR/assets/content/docs/images/spatial-windows.png'));
  assert.deepEqual(localizedImage, await fs.readFile(path.join(repository, 'docs/images/spatial-windows-fr.png')));
});

test('a translation with a missing document is rejected rather than partially published', async t => {
  const {root, target} = await translatedFixture(t);
  const policy = await fs.readFile(path.join(target, 'docs/privacy-policy.md'));
  await fs.unlink(path.join(target, 'docs/privacy-policy.md'));
  await assert.rejects(generate(root, path.join(root, 'output')), /Incomplete translation fr-FR: missing docs\/privacy-policy\.md/);
  await fs.writeFile(path.join(target, 'docs/privacy-policy.md'), policy);
  await fs.unlink(path.join(target, 'docs/nilesoft-shell.md'));
  await assert.rejects(generate(root, path.join(root, 'output')), /Incomplete translation fr-FR: missing docs\/nilesoft-shell\.md/);
});

test('missing localized media and mismatched translation headings fail the build', async t => {
  const {root, target} = await translatedFixture(t);
  const image = path.join(root, 'docs/images/spatial-windows-fr.png');
  const bytes = await fs.readFile(image);
  await fs.unlink(image);
  await assert.rejects(generate(root, path.join(root, 'output')), /Missing localized media \(fr-FR\)/);
  await fs.writeFile(image, bytes);
  await fs.appendFile(path.join(target, 'docs/user-guide.md'), '\n## Une rubrique supplémentaire\n');
  await assert.rejects(generate(root, path.join(root, 'output')), /Translation heading (?:structure|count) differs from English/);
});

test('translated search keeps Greek and accented words through indexing and querying', async () => {
  const hook = (await fs.readFile(path.join(repository, 'website/_includes/lunr/custom-index.js'), 'utf8'))
    .replace(/{%[^]*?%}/g, '');
  const customize = new Function('lunr', 'docs', 'i', hook);
  const docs = {
    0: {title: 'Μετακίνηση και αλλαγή μεγέθους', content: 'Κλειδώστε την αποθηκευμένη διάταξη.'},
    1: {title: 'Déplacement et redimensionnement', content: 'Éléments et sélection.'},
    2: {title: 'Verschieben und Größenänderung', content: 'Fenstergrößen ändern.'},
    3: {title: 'Movimiento y cambio de tamaño', content: 'Configuración y selección.'},
  };
  const index = lunr(function () {
    this.ref('id');
    this.field('title');
    this.field('content');
    for (const i in docs) {
      customize.call(this, lunr, docs, i);
      this.add({id: i, ...docs[i]});
    }
  });
  for (const [query, expected] of [['μετακίνηση', '0'], ['αποθηκευμένη', '0'], ['déplacement', '1'], ['éléments', '1'], ['größenänderung', '2'], ['tamaño', '3'], ['configuración', '3']]) {
    assert.equal(index.search(query)[0]?.ref, expected, query);
  }
});
