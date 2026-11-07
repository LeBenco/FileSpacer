import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import {Marked} from 'marked';
import {fetchReleaseDates, releaseHeading} from './releases.mjs';

const template = path.dirname(fileURLToPath(import.meta.url));
const navigationDepth = 2; // Maximum menu levels, counting each document as level 1.
const sources = [
  ['README.md', 'overview'],
  ['docs/user-guide.md', 'user-guide'],
  ['CHANGELOG.md', 'changelog'],
  ['THIRD_PARTY_NOTICES.md', 'third-party-notices'],
];
const localizedReferences = ['docs/nilesoft-shell.md', 'docs/privacy-policy.md'];
const technicalReferences = [
  'BUILD.md', 'docs/README.md', 'docs/build.md', 'docs/native-shell-view.md',
  'docs/context-menu-integration.md', 'docs/search-integration.md',
  'docs/title-menu.md', 'docs/window-shortcuts.md', 'docs/winget.md',
  'tests/client/README.md', 'tests/regression/README.md',
  'website/README.md', 'website/translations/README.md',
];
const privacyAliases = {
  'user-guide--updates-and-troubleshooting': ['privacy-policy--update-checks', 'privacy-policy--local-data', 'privacy-policy--resetting-preferences'],
  'user-guide--privacy': ['privacy-policy', 'privacy-policy--windows-and-external-applications'],
};
const escape = value => String(value).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
const text = value => value.replace(/<[^>]*>/g, '').replace(/[*`_~]/g, '');
const slug = value => text(value).toLowerCase().replace(/[^\p{L}\p{M}\p{N}_\-\s]/gu, '').replace(/\s/g, '-');
const json = value => JSON.stringify(value, null, 2) + '\n';

async function exists(file) {
  try { await fs.access(file); return true; }
  catch (error) { if (error.code === 'ENOENT') return false; throw error; }
}

function navigation(headings, ui) {
  const root = {...headings[0], children: []};
  const stack = [root];
  for (const heading of headings.slice(1)) {
    while (stack.length > 1 && stack.at(-1).depth >= heading.depth) stack.pop();
    if (stack.length >= navigationDepth) continue;
    const node = {...heading, children: []};
    stack.at(-1).children.push(node);
    stack.push(node);
  }
  function item(node) {
    const expand = node.children.length ? `<button class="nav-list-expander btn-reset" aria-label="${escape(ui.submenu + ' ' + node.title)}" aria-expanded="false"><svg viewBox="0 0 24 24" aria-hidden="true"><use xlink:href="#svg-arrow-right"></use></svg></button>` : '';
    const children = node.children.length ? `<ul class="nav-list">${node.children.map(item).join('')}</ul>` : '';
    return `<li class="nav-list-item">${expand}<a href="#${escape(node.id)}" class="nav-list-link">${escape(node.title)}</a>${children}</li>`;
  }
  return item(root);
}

function languagePicker(current, languages) {
  const options = languages.map(language => `<a href="${escape(language.url)}" data-language-href="${escape(language.url)}" hreflang="${language.code}" lang="${language.code}"${language.code === current.code ? ' aria-current="true"' : ''}><span class="language-flag" aria-hidden="true">${language.flag}</span>${escape(language.name)}${language.code === current.code ? '<span class="selected-mark" aria-hidden="true">✓</span>' : ''}</a>`).join('');
  return `<details class="language-picker"><summary aria-label="${escape(current.ui.language + ' : ' + current.name)}"><span class="language-flag" aria-hidden="true">${current.flag}</span><span>${escape(current.name)}</span><svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="2"><path d="m6 9 6 6 6-6"/></svg></summary><div class="language-options">${options}</div></details>`;
}

function describeLanguage(code, ui, root, base, primary = false) {
  const locale = new Intl.Locale(code);
  if (!locale.region || !/^[a-z]{2}-[A-Z]{2}$/.test(code)) throw new Error(`Use a language-region locale directory: ${code}`);
  const flag = [...locale.region].map(letter => String.fromCodePoint(0x1f1e6 + letter.charCodeAt(0) - 65)).join('');
  return {code, ui, root, flag, name: ui.name, path: primary ? '' : locale.language, source: primary ? '.' : `_locales/${code}`, url: base + (primary ? '/' : `/${locale.language}/`)};
}

function sourceFile(language, source) {
  if (!language.modern) return source;
  return source.startsWith('docs/')
    ? (language.primary ? 'docs/en/' : '') + source.slice(5) : source;
}

export async function generate(repository = path.dirname(template), destination = path.join(repository, 'build/site-source'), options = {}) {
  const config = (await fs.readFile(path.join(template, '_config.yml'), 'utf8')).replaceAll('\r\n', '\n');
  const base = config.match(/^baseurl:\s*(.*)$/m)[1].trim().replace(/\/$/, '');
  const english = JSON.parse(await fs.readFile(path.join(template, 'locale.json'), 'utf8'));
  const languages = [describeLanguage(english.locale, english, repository, base, true)];
  languages[0].primary = true;
  languages[0].modern = await exists(path.join(repository, 'docs/en/user-guide.md'));
  const modernTranslations = languages[0].modern;
  const translations = path.join(repository, modernTranslations ? 'docs' : 'website/translations');
  if (await exists(translations)) {
    for (const entry of (await fs.readdir(translations, {withFileTypes: true})).sort((a, b) => a.name.localeCompare(b.name, 'en'))) {
      if (!entry.isDirectory()) continue;
      const root = path.join(translations, entry.name);
      if (modernTranslations && !await exists(path.join(root, 'locale.json'))) continue;
      for (const file of ['locale.json', ...sources.map(([source]) => modernTranslations ? source.replace(/^docs\//, '') : source), ...localizedReferences.map(source => modernTranslations ? source.replace(/^docs\//, '') : source)]) {
        if (!await exists(path.join(root, file))) throw new Error(`Incomplete translation ${entry.name}: missing ${file}`);
      }
      const ui = JSON.parse(await fs.readFile(path.join(root, 'locale.json'), 'utf8'));
      for (const key of Object.keys(english).filter(key => key !== 'locale')) {
        if (typeof ui[key] !== 'string' || !ui[key].trim()) throw new Error(`Missing interface translation ${entry.name}: ${key}`);
      }
      const language = describeLanguage(modernTranslations ? ui.locale : entry.name, ui, root, base);
      language.modern = modernTranslations;
      languages.push(language);
    }
  }
  if (new Set(languages.map(language => language.path)).size !== languages.length) throw new Error('Duplicate language routes');
  for (const locale of options.recordTranslations || []) {
    if (!languages.some(language => language.code === locale && language !== languages[0])) throw new Error(`Unknown translation locale: ${locale}`);
  }
  const parser = new Marked({gfm: true});
  const canonical = new Map();
  const results = [];
  const warnings = [], snapshots = [];
  for (const language of languages) {
    language.references = new Set(localizedReferences);
    if (language !== languages[0]) {
      for (const reference of technicalReferences) {
        if (await exists(path.join(language.root, reference))) language.references.add(reference);
      }
    }
    const documents = new Map();
    const record = language !== languages[0] && options.recordTranslations
      && (!options.recordTranslations.length || options.recordTranslations.includes(language.code));
    const stateFile = path.join(language.root, 'source-state.json');
    const state = language !== languages[0] && !record && await exists(stateFile)
      ? JSON.parse(await fs.readFile(stateFile, 'utf8')).sources : {};
    for (const [source, id] of sources) {
      const content = await fs.readFile(path.join(language.root, sourceFile(language, source)), 'utf8');
      const tokens = parser.lexer(content);
      const sourceHash = createHash('sha256').update(content.replaceAll('\r\n', '\n')).digest('hex');
      const baseline = state[source];
      const reference = baseline ? baseline.headings.map(([anchor, depth], index) => ({
        anchor, depth, id: index === 0 ? id : `${id}--${anchor}`,
      })) : canonical.get(source)?.headings;
      if (baseline && baseline.sourceHash !== canonical.get(source).sourceHash) {
        const message = `${language.code}/${source}: English has changed since this translation was synchronized; retaining the existing translation and section links.`;
        warnings.push({language: language.code, source, message});
        if (options.onWarning) options.onWarning(message);
        else console.warn(process.env.GITHUB_ACTIONS ? `::warning file=${path.relative(repository, path.join(language.root, sourceFile(language, source))).split(path.sep).join('/')}::${message}` : `WARNING: ${message}`);
      }
      const anchors = new Map(), counts = new Map(), headings = [];
      parser.walkTokens(tokens, token => {
        if (token.type !== 'heading') return;
        const original = slug(token.text), count = counts.get(original) || 0;
        counts.set(original, count + 1);
        const originalId = original + (count ? `-${count}` : '');
        const first = headings.length === 0 && token.depth === 1;
        const target = reference?.[headings.length];
        if (language !== languages[0] && (!target || target.depth !== Math.min(6, token.depth + 1))) throw new Error(`Translation heading structure differs from English: ${language.code}/${source}`);
        token.siteId = target?.id || (first ? id : `${id}--${originalId}`);
        token.siteDepth = Math.min(6, token.depth + 1);
        token.siteTitle = first ? language.ui[id] : null;
        if (source === 'CHANGELOG.md' && token.depth === 2 && options.releaseDates) {
          token.siteTitle = releaseHeading(text(token.text), language.code, options.releaseDates);
        }
        anchors.set(originalId, token.siteId);
        headings.push({id: token.siteId, anchor: originalId, title: token.siteTitle || text(token.text), depth: token.siteDepth});
      });
      if (headings[0]?.depth !== 2) throw new Error(`Missing top-level title: ${source}`);
      if (language !== languages[0]) {
        if (headings.length !== reference.length) throw new Error(`Translation heading count differs from its source reference: ${language.code}/${source}. Synchronize the translation, then record its sources.`);
        for (const target of reference) if (!anchors.has(target.anchor)) anchors.set(target.anchor, target.id);
      }
      // Explicit HTML anchors are valid Markdown targets too. Namespace them
      // by document so that independently authored pages can reuse an ID.
      parser.walkTokens(tokens, token => {
        if (token.type !== 'html') return;
        for (const match of token.text.matchAll(/\sid\s*=\s*(["'])([^"']+)\1/gi)) {
          if (anchors.has(match[2])) throw new Error(`Duplicate source anchor: ${source}#${match[2]}`);
          anchors.set(match[2], `${id}--${match[2]}`);
        }
      });
      documents.set(source, {source, id, tokens, anchors, headings, sourceHash});
    }
    if (language === languages[0]) for (const [source, doc] of documents) canonical.set(source, doc);
    if (record) snapshots.push([stateFile, {version: 1, sources: Object.fromEntries([...canonical].map(([source, doc]) => [source, {
      sourceHash: doc.sourceHash, headings: doc.headings.map(heading => [heading.anchor, heading.depth]),
    }]))}]);
    const assets = new Map();
    function resolve(source, href, image = false) {
      if (/^(?:[a-z][a-z\d+.-]*:|\/\/)/i.test(href)) return href;
      const at = href.indexOf('#');
      const file = decodeURIComponent(at < 0 ? href : href.slice(0, at)).replaceAll('\\', '/');
      const fragment = at < 0 ? '' : decodeURIComponent(href.slice(at + 1));
      if (language.modern) {
        const relativeRoot = path.relative(repository, language.root).replaceAll('\\', '/');
        const actualSource = path.posix.join(relativeRoot, sourceFile(language, source));
        const actualTarget = file ? path.posix.normalize(path.posix.join(path.posix.dirname(actualSource), file)) : actualSource;
        if (actualTarget.startsWith('../') || path.posix.isAbsolute(actualTarget)) throw new Error(`Path outside repository: ${href}`);
        if (image) {
          const output = `assets/content/${actualTarget}`;
          assets.set(actualTarget, output);
          return output;
        }
        const entry = [...documents].find(([logical]) => path.posix.join(relativeRoot, sourceFile(language, logical)) === actualTarget);
        if (!entry && !fragment && actualTarget === path.posix.join(relativeRoot, sourceFile(language, 'docs/privacy-policy.md'))) return '#user-guide--privacy';
        if (!entry) return 'https://github.com/LeBenco/FileSpacer/blob/main/' + actualTarget.split('/').map(encodeURIComponent).join('/') + (fragment ? '#' + encodeURIComponent(fragment) : '');
        const doc = entry[1];
        const anchor = fragment ? doc.anchors.get(fragment) : doc.id;
        if (!anchor) throw new Error(`Missing heading in ${language.code}/${source}: ${href}`);
        return '#' + anchor;
      }
      const target = file ? path.posix.normalize(path.posix.join(path.posix.dirname(source), file)) : source;
      if (target.startsWith('../') || path.posix.isAbsolute(target)) throw new Error(`Path outside repository: ${href}`);
      if (image) {
        const localized = language === languages[0] || !/\.(png|gif)$/i.test(target) ? target : target.replace(/(\.[^.]+)$/, `-${language.path}$1`);
        const output = `assets/content/${target}`;
        assets.set(localized, output);
        return output;
      }
      const doc = documents.get(target);
      if (!doc) {
        if (target === 'docs/privacy-policy.md' && !fragment) return '#user-guide--privacy';
        const reference = language !== languages[0] && language.references.has(target)
          ? `website/translations/${language.code}/${target}` : target;
        return 'https://github.com/LeBenco/FileSpacer/blob/main/' + reference.split('/').map(encodeURIComponent).join('/') + (fragment ? '#' + encodeURIComponent(fragment) : '');
      }
      const anchor = fragment ? doc.anchors.get(fragment) : doc.id;
      if (!anchor) throw new Error(`Missing heading in ${language.code}/${source}: ${href}`);
      return '#' + anchor;
    }
    const rendered = [];
    for (const doc of documents.values()) {
      parser.walkTokens(doc.tokens, token => {
        if (token.type === 'link' || token.type === 'image') token.href = resolve(doc.source, token.href, token.type === 'image');
        if (token.type === 'html') token.text = token.text
          .replace(/(\s)id\s*=\s*(["'])([^"']+)\2/gi, (_, space, quote, anchor) => `${space}id=${quote}${escape(doc.anchors.get(anchor))}${quote}`)
          .replace(/(<(?:a|img)\b[^>]*\b(href|src)\s*=\s*)(["'])(.*?)\3/gi, (_, before, attribute, quote, href) => before + quote + resolve(doc.source, href, attribute.toLowerCase() === 'src') + quote);
      });
      const renderer = new Marked({gfm: true, renderer: {
        heading(token) {
          const title = token.siteTitle ? escape(token.siteTitle) : this.parser.parseInline(token.tokens);
          const aliases = (privacyAliases[token.siteId] || []).map(id => `<span id="${id}" data-alias-for="${token.siteId}" aria-hidden="true"></span>`).join('');
          return `${aliases}<h${token.siteDepth} id="${escape(token.siteId)}"><a href="#${escape(token.siteId)}" class="anchor-heading" aria-labelledby="${escape(token.siteId)}"><svg viewBox="0 0 16 16" aria-hidden="true"><use xlink:href="#svg-link"></use></svg></a>${title}</h${token.siteDepth}>\n`;
        },
      }});
      rendered.push(`<section aria-labelledby="${doc.id}" data-source="${doc.source}">\n${renderer.parser(doc.tokens)}</section>`);
    }
    let html = `<h1 class="fs-9">FileSpacer</h1>\n${rendered.join('\n<hr>\n')}`.replace(/<img\b[^>]*>/g, img => {
      const src = img.match(/\bsrc="([^"]+)"/)?.[1];
      if (!src || !/\.(png|gif)$/i.test(src)) return img;
      const alt = img.match(/\balt="([^"]*)"/)?.[1] || '';
      return `<a class="screenshot-link" href="${src}" aria-label="${escape(language.ui.enlargeImage)}: ${alt}">${img}</a>`;
    });
    const ids = [...html.matchAll(/\sid\s*=\s*(["'])([^"']+)\1/g)].map(match => match[2]);
    const missing = [...html.matchAll(/<a\b[^>]*\shref\s*=\s*(["'])#([^"']+)\1/g)].filter(match => !ids.includes(decodeURIComponent(match[2])));
    if (new Set(ids).size !== ids.length || missing.length) throw new Error('Duplicate heading IDs or broken internal links');
    const output = path.join(destination, language.source);
    await fs.mkdir(output, {recursive: true});
    await fs.cp(path.join(template, '_includes'), path.join(output, '_includes'), {recursive: true});
    await fs.cp(path.join(template, '_layouts'), path.join(output, '_layouts'), {recursive: true});
    await fs.cp(path.join(template, 'assets'), path.join(output, 'assets'), {recursive: true});
    let localizedConfig = config.replace(/^baseurl:.*$/m, 'baseurl: ' + language.url.replace(/\/$/, ''))
      .replace(/^description:.*$/m, 'description: ' + JSON.stringify(language.ui.description))
      .replace(/^back_to_top_text:.*$/m, 'back_to_top_text: ' + JSON.stringify(language.ui.back))
      .replace(/^aux_links:\n[\s\S]*?(?=^[^\s])/m, `aux_links:\n  ${JSON.stringify(language.ui.repository)}:\n    - https://github.com/LeBenco/FileSpacer\n`);
    localizedConfig += `\nlang: ${language.code}\nfs_ui: ${JSON.stringify(language.ui)}\n`;
    await fs.writeFile(path.join(output, '_config.yml'), localizedConfig);
    await fs.writeFile(path.join(output, '_includes/components/site_nav.html'), `<nav aria-label="${escape(language.ui.navigation)}" id="site-nav" class="site-nav">${languagePicker(language, languages)}<ul class="nav-list">${[...documents.values()].map(doc => navigation(doc.headings, language.ui)).join('')}</ul></nav>\n`);
    for (const [source, target] of assets) {
      if (!await exists(path.join(repository, source))) throw new Error(`Missing localized media (${language.code}): ${source}`);
      await fs.mkdir(path.dirname(path.join(output, target)), {recursive: true});
      await fs.copyFile(path.join(repository, source), path.join(output, target));
      if (/\.(png|gif)$/i.test(target)) {
        const version = createHash('sha256').update(await fs.readFile(path.join(repository, source))).digest('hex').slice(0, 12);
        html = html.replaceAll(`="${target}"`, `="${target}?v=${version}"`);
      }
    }
    await fs.writeFile(path.join(output, 'index.html'), `---\nlayout: default\ntitle: FileSpacer\nhas_toc: false\n---\n${html}`);
    results.push({code: language.code, path: language.path, source: language.source, documents: documents.size, headings: ids.length, images: assets.size});
  }
  await fs.writeFile(path.join(destination, 'locales.json'), json(results));
  for (const [file, snapshot] of snapshots) await fs.writeFile(file, json(snapshot));
  return {...results[0], languages: results, warnings, destination};
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2);
  if (args.length && args[0] !== '--record-translation-sources') throw new Error('Usage: node website/generate.mjs [--record-translation-sources [locale...]]');
  console.log(json(await generate(undefined, undefined, {
    releaseDates: await fetchReleaseDates(),
    ...(args.length ? {recordTranslations: args.slice(1)} : {}),
  })));
}
