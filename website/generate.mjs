import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {Marked} from 'marked';

const template = path.dirname(fileURLToPath(import.meta.url));
const sources = [
  ['README.md', 'overview'],
  ['docs/user-guide.md', 'user-guide'],
  ['CHANGELOG.md', 'changelog'],
  ['THIRD_PARTY_NOTICES.md', 'third-party-notices'],
];
const localizedReferences = ['docs/nilesoft-shell.md', 'docs/privacy-policy.md'];
const privacyAliases = {
  'user-guide--settings-and-updates': ['privacy-policy--update-checks'],
  'user-guide--local-data': ['privacy-policy--local-data', 'privacy-policy--resetting-preferences'],
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

export async function generate(repository = path.dirname(template), destination = path.join(repository, 'build/site-source')) {
  const changelog = await fs.readFile(path.join(repository, 'CHANGELOG.md'), 'utf8');
  const version = changelog.match(/^##\s+\[?v?(\d+\.\d+\.\d+(?:-[\w.-]+)?)\]?(?=\s|$)/m)?.[1];
  if (!version) throw new Error('Missing released version in CHANGELOG.md');
  const config = (await fs.readFile(path.join(template, '_config.yml'), 'utf8')).replaceAll('\r\n', '\n');
  const base = config.match(/^baseurl:\s*(.*)$/m)[1].trim().replace(/\/$/, '');
  const english = JSON.parse(await fs.readFile(path.join(template, 'locale.json'), 'utf8'));
  english.download = english.download.replace('{version}', 'v' + version);
  const languages = [describeLanguage(english.locale, english, repository, base, true)];
  const translations = path.join(repository, 'website/translations');
  if (await exists(translations)) {
    for (const entry of (await fs.readdir(translations, {withFileTypes: true})).sort((a, b) => a.name.localeCompare(b.name, 'en'))) {
      if (!entry.isDirectory()) continue;
      const root = path.join(translations, entry.name);
      for (const file of ['locale.json', ...sources.map(([source]) => source), ...localizedReferences]) {
        if (!await exists(path.join(root, file))) throw new Error(`Incomplete translation ${entry.name}: missing ${file}`);
      }
      const ui = JSON.parse(await fs.readFile(path.join(root, 'locale.json'), 'utf8'));
      for (const key of Object.keys(english).filter(key => key !== 'locale')) {
        if (typeof ui[key] !== 'string' || !ui[key].trim()) throw new Error(`Missing interface translation ${entry.name}: ${key}`);
      }
      ui.download = ui.download.replace('{version}', 'v' + version);
      languages.push(describeLanguage(entry.name, ui, root, base));
    }
  }
  if (new Set(languages.map(language => language.path)).size !== languages.length) throw new Error('Duplicate language routes');
  const parser = new Marked({gfm: true});
  const canonical = new Map();
  const results = [];
  for (const language of languages) {
    const documents = new Map();
    for (const [source, id] of sources) {
      const tokens = parser.lexer(await fs.readFile(path.join(language.root, source), 'utf8'));
      const anchors = new Map(), counts = new Map(), headings = [];
      parser.walkTokens(tokens, token => {
        if (token.type !== 'heading') return;
        const original = slug(token.text), count = counts.get(original) || 0;
        counts.set(original, count + 1);
        const originalId = original + (count ? `-${count}` : '');
        const first = headings.length === 0 && token.depth === 1;
        const target = canonical.get(source)?.headings[headings.length];
        if (language !== languages[0] && (!target || target.depth !== Math.min(6, token.depth + 1))) throw new Error(`Translation heading structure differs from English: ${language.code}/${source}`);
        token.siteId = target?.id || (first ? id : `${id}--${originalId}`);
        token.siteDepth = Math.min(6, token.depth + 1);
        token.siteTitle = first ? language.ui[id] : null;
        anchors.set(originalId, token.siteId);
        headings.push({id: token.siteId, title: token.siteTitle || text(token.text), depth: token.siteDepth});
      });
      if (headings[0]?.depth !== 2) throw new Error(`Missing top-level title: ${source}`);
      if (language !== languages[0]) {
        if (headings.length !== canonical.get(source).headings.length) throw new Error(`Translation heading count differs from English: ${language.code}/${source}`);
        for (const [anchor, target] of canonical.get(source).anchors) if (!anchors.has(anchor)) anchors.set(anchor, target);
      }
      documents.set(source, {source, id, tokens, anchors, headings});
    }
    if (language === languages[0]) for (const [source, doc] of documents) canonical.set(source, doc);
    const assets = new Map();
    function resolve(source, href, image = false) {
      if (/^(?:[a-z][a-z\d+.-]*:|\/\/)/i.test(href)) return href;
      const at = href.indexOf('#');
      const file = decodeURIComponent(at < 0 ? href : href.slice(0, at)).replaceAll('\\', '/');
      const fragment = at < 0 ? '' : decodeURIComponent(href.slice(at + 1));
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
        const reference = language !== languages[0] && localizedReferences.includes(target)
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
        if (token.type === 'html') token.text = token.text.replace(/(<img\b[^>]*\bsrc=")([^"]+)(")/g, (_, before, href, after) => before + resolve(doc.source, href, true) + after);
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
    const html = `<h1 class="fs-9">FileSpacer</h1>\n${rendered.join('\n<hr>\n')}`.replace(/<img\b[^>]*>/g, img => {
      const src = img.match(/\bsrc="([^"]+)"/)?.[1];
      if (!src || !/\.(png|gif)$/i.test(src)) return img;
      const alt = img.match(/\balt="([^"]*)"/)?.[1] || '';
      return `<a class="screenshot-link" href="${src}" aria-label="${escape(language.ui.enlargeImage)}: ${alt}">${img}</a>`;
    });
    const ids = [...html.matchAll(/\bid="([^"]+)"/g)].map(match => match[1]);
    const missing = [...html.matchAll(/<a\b[^>]*href="#([^"]+)"/g)].filter(match => !ids.includes(match[1]));
    if (new Set(ids).size !== ids.length || missing.length) throw new Error('Duplicate heading IDs or broken internal links');
    const output = path.join(destination, language.source);
    await fs.mkdir(output, {recursive: true});
    await fs.cp(path.join(template, '_includes'), path.join(output, '_includes'), {recursive: true});
    await fs.cp(path.join(template, '_layouts'), path.join(output, '_layouts'), {recursive: true});
    await fs.cp(path.join(template, 'assets'), path.join(output, 'assets'), {recursive: true});
    let localizedConfig = config.replace(/^baseurl:.*$/m, 'baseurl: ' + language.url.replace(/\/$/, ''))
      .replace(/^description:.*$/m, 'description: ' + JSON.stringify(language.ui.description))
      .replace(/^back_to_top_text:.*$/m, 'back_to_top_text: ' + JSON.stringify(language.ui.back))
      .replace(/^aux_links:\n[\s\S]*?(?=^[^\s])/m, `aux_links:\n  ${JSON.stringify(language.ui.download)}:\n    - https://github.com/LeBenco/FileSpacer/releases/latest\n  ${JSON.stringify(language.ui.repository)}:\n    - https://github.com/LeBenco/FileSpacer\n`);
    localizedConfig += `\nlang: ${language.code}\nfs_ui: ${JSON.stringify(language.ui)}\n`;
    await fs.writeFile(path.join(output, '_config.yml'), localizedConfig);
    await fs.writeFile(path.join(output, '_includes/components/site_nav.html'), `<nav aria-label="${escape(language.ui.navigation)}" id="site-nav" class="site-nav">${languagePicker(language, languages)}<ul class="nav-list">${[...documents.values()].map(doc => navigation(doc.headings, language.ui)).join('')}</ul></nav>\n`);
    await fs.writeFile(path.join(output, 'index.html'), `---\nlayout: default\ntitle: FileSpacer\nhas_toc: false\n---\n${html}`);
    for (const [source, target] of assets) {
      if (!await exists(path.join(repository, source))) throw new Error(`Missing localized media (${language.code}): ${source}`);
      await fs.mkdir(path.dirname(path.join(output, target)), {recursive: true});
      await fs.copyFile(path.join(repository, source), path.join(output, target));
    }
    results.push({code: language.code, path: language.path, source: language.source, documents: documents.size, headings: ids.length, images: assets.size});
  }
  await fs.writeFile(path.join(destination, 'locales.json'), json(results));
  return {...results[0], languages: results, destination};
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  console.log(json(await generate()));
}
