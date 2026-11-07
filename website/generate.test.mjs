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
    const actual = source.startsWith('docs/') ? source.replace('docs/', 'docs/en/') : source;
    let content = await fs.readFile(path.join(repository, actual));
    if (source.endsWith('.md')) content = Buffer.from(content.toString('utf8').replaceAll('docs/en/tests/', 'tests/').replaceAll('docs/en/website/', 'website/').replaceAll('docs/en/', 'docs/'));
    await fs.writeFile(path.join(root, source), content);
  }
  await fs.cp(path.join(repository, 'docs/en/images'), path.join(root, 'docs/images'), {recursive: true});
  await fs.copyFile(path.join(repository, 'docs/fr/images/spatial-windows-fr.png'), path.join(root, 'docs/images/spatial-windows-fr.png'));
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
  assert.match(html, /https:\/\/github\.com\/LeBenco\/FileSpacer\/blob\/main\/docs\/en\/build\.md/);
  assert.match(html, /https:\/\/github\.com\/LeBenco\/FileSpacer\/blob\/main\/docs\/en\/tests\/regression\/README\.md/);
  assert.doesNotMatch(html, /data-source="(?:docs\/(?:build|native-shell-view)|tests\/)/);
  assert.match(nav, /href="#user-guide--customization"/);
  assert.doesNotMatch(nav, /href="#(?:user-guide--(?:integration|windows-integration|everything|nilesoft-shell)|overview--copyright-and-license)"/);
  assert.match(html, /<div id="user-guide--everything"><\/div>/);
  const windows = html.match(/<h3 id="user-guide--window-navigation-and-layout">[^]*?(?=<h3 |<\/section>)/)?.[0];
  assert.ok(windows, 'The user guide groups window navigation and locking');
  assert.match(windows, /<div id="user-guide--locking-a-saved-layout"><\/div>/);
  assert.match(html, /<div id="user-guide--moving-and-resizing"><\/div>/);
  assert.match(html, /href="#user-guide--locking-a-saved-layout"/);
  assert.match(html, /assets\/content\/src\/res\/FileSpacer\.svg/);
  assert.doesNotMatch(nav, /href="#privacy-policy"/);
  assert.match(nav, /href="#user-guide--privacy"/);
  for (const [alias, target] of [
    ['privacy-policy', 'privacy'], ['privacy-policy--local-data', 'updates-and-troubleshooting'],
    ['privacy-policy--resetting-preferences', 'updates-and-troubleshooting'],
    ['privacy-policy--update-checks', 'updates-and-troubleshooting'],
    ['privacy-policy--windows-and-external-applications', 'privacy'],
  ]) assert.ok(html.includes(`id="${alias}" data-alias-for="user-guide--${target}"`));
  const screenshots = [...html.matchAll(/<a class="screenshot-link"[^>]+><img\b[^>]+><\/a>/g)];
  assert.equal(screenshots.length, [...html.matchAll(/<img\b[^>]*src="[^"]+\.(?:png|gif)\?v=[a-f0-9]{12}"/g)].length);
  assert.ok(screenshots.length > 0);
  assert.ok(screenshots.every(([link]) => !link.includes('.svg')));
  assert.match(html, /screenshot-link" href="assets\/content\/docs\/en\/images\/spatial-windows\.png\?v=[a-f0-9]{12}"/);
  await fs.access(path.join(output, 'assets/content/src/res/FileSpacer.svg'));
});

test('GitHub publication dates localize changelog headings and navigation without changing notes or existing anchors', async t => {
  const output = await fs.mkdtemp(path.join(os.tmpdir(), 'filespacer-release-dates-'));
  t.after(() => {
    assert.equal(path.dirname(output), os.tmpdir());
    return fs.rm(output, {recursive: true, force: true});
  });
  const baseline = await generate(repository, output, {onWarning: () => {}});
  const oldIds = new Map();
  const ids = html => [...html.matchAll(/\sid="([^"]+)"/g)].map(match => match[1]);
  for (const language of baseline.languages) {
    oldIds.set(language.code, ids(await fs.readFile(path.join(output, language.source, 'index.html'), 'utf8')));
  }
  const result = await generate(repository, output, {
    onWarning: () => {}, releaseDates: {'1.3.2': '2026-11-05T23:59:59Z'},
  });
  for (const language of result.languages) {
    const html = await fs.readFile(path.join(output, language.source, 'index.html'), 'utf8');
    const nav = await fs.readFile(path.join(output, language.source, '_includes/components/site_nav.html'), 'utf8');
    assert.deepEqual(ids(html), oldIds.get(language.code), language.code);
    const date = new Intl.DateTimeFormat(language.code, {year: 'numeric', month: '2-digit', day: '2-digit', timeZone: 'UTC'})
      .format(new Date('2026-11-05T23:59:59Z'));
    assert.ok(html.includes(`1.3.2 — ${date}</h3>`), language.code);
    assert.ok(nav.includes(`1.3.2 — ${date}</a>`), language.code);
    assert.match(html, /1\.3\.1<\/h3>/, 'A version not yet published on GitHub is shown without an invented date');
    if (language.code === 'fr-FR') assert.match(html, /Ajouter le polonais/);
    if (language.code === 'en-GB') assert.match(html, /Add Polish/);
  }
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
  assert.equal(imageUrl(after, 'selected-name'), imageUrl(before, 'selected-name'));
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

test('language directories use the root English README and require their own images', async t => {
  const root = await fixture(t);
  await fs.rename(path.join(root, 'docs'), path.join(root, 'english'));
  await fs.mkdir(path.join(root, 'docs'));
  await fs.rename(path.join(root, 'english'), path.join(root, 'docs/en'));
  const readme = path.join(root, 'README.md');
  await fs.writeFile(readme, (await fs.readFile(readme, 'utf8')).replaceAll('docs/', 'docs/en/'));
  await fs.appendFile(readme, '\nCanonical root README.\n');
  await fs.writeFile(path.join(root, 'docs/en/README.md'), '# Technical index\n\nNot the project overview.\n');
  await fs.cp(path.join(repository, 'docs/fr'), path.join(root, 'docs/fr'), {recursive: true});
  const output = path.join(root, 'output');
  await generate(root, output, {onWarning: () => {}});
  const english = await fs.readFile(path.join(output, 'index.html'), 'utf8');
  assert.match(english, /Canonical root README/);
  assert.doesNotMatch(english, /Not the project overview/);
  const french = await fs.readFile(path.join(output, '_locales/fr-FR/index.html'), 'utf8');
  assert.match(french, /assets\/content\/docs\/fr\/images\/spatial-windows-fr\.png/);
  assert.doesNotMatch(french, /assets\/content\/docs\/en\/images\//);
  await fs.unlink(path.join(root, 'docs/fr/images/spatial-windows-fr.png'));
  await assert.rejects(generate(root, output, {onWarning: () => {}}), /Missing localized media \(fr-FR\)/);
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
  await fs.cp(path.join(repository, 'docs/fr'), target, {recursive: true});
  await fs.mkdir(path.join(target, 'docs'), {recursive: true});
  await fs.copyFile(path.join(repository, 'docs/fr/nilesoft-shell.md'), path.join(target, 'docs/nilesoft-shell.md'));
  await fs.copyFile(path.join(repository, 'docs/fr/privacy-policy.md'), path.join(target, 'docs/privacy-policy.md'));
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
      const state = JSON.parse(await fs.readFile(path.join(repository, 'docs/' + language.path, 'source-state.json'), 'utf8'));
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
    const reference = `docs/${language.path || 'en'}/nilesoft-shell.md`;
    assert.ok(html.includes('href="https://github.com/LeBenco/FileSpacer/blob/main/' + reference + '"'), 'Nilesoft reference must use the selected language');
    await fs.access(path.join(repository, reference));
  }
  const localizedImage = await fs.readFile(path.join(output, '_locales/fr-FR/assets/content/docs/fr/images/spatial-windows-fr.png'));
  assert.deepEqual(localizedImage, await fs.readFile(path.join(repository, 'docs/fr/images/spatial-windows-fr.png')));
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
