import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {Marked} from 'marked';

const template = path.dirname(fileURLToPath(import.meta.url));
const sources = [
  ['README.md', 'overview', 'Overview'],
  ['docs/user-guide.md', 'user-guide', 'User guide'],
  ['docs/privacy-policy.md', 'privacy-policy', 'Privacy policy'],
  ['CHANGELOG.md', 'changelog', 'Changelog'],
  ['THIRD_PARTY_NOTICES.md', 'third-party-notices', 'Third-party notices'],
];
const escape = value => String(value).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
const text = value => value.replace(/<[^>]*>/g, '').replace(/[*`_~]/g, '');
const slug = value => text(value).toLowerCase().replace(/[^\p{L}\p{M}\p{N}_\-\s]/gu, '').replace(/\s/g, '-');

function navigation(headings) {
  const root = {...headings[0], children: []};
  const stack = [root];
  for (const heading of headings.slice(1)) {
    while (stack.length > 1 && stack.at(-1).depth >= heading.depth) stack.pop();
    const node = {...heading, children: []};
    stack.at(-1).children.push(node);
    stack.push(node);
  }
  function item(node) {
    const expand = node.children.length ? `<button class="nav-list-expander btn-reset" aria-label="${escape(node.title)} submenu" aria-expanded="false"><svg viewBox="0 0 24 24" aria-hidden="true"><use xlink:href="#svg-arrow-right"></use></svg></button>` : '';
    const children = node.children.length ? `<ul class="nav-list">${node.children.map(item).join('')}</ul>` : '';
    return `<li class="nav-list-item">${expand}<a href="#${escape(node.id)}" class="nav-list-link">${escape(node.title)}</a>${children}</li>`;
  }
  return item(root);
}

export async function generate(repository = path.dirname(template), destination = path.join(repository, 'build/site-source')) {
  const parser = new Marked({gfm: true});
  const documents = new Map();
  for (const [source, id, title] of sources) {
    const tokens = parser.lexer(await fs.readFile(path.join(repository, source), 'utf8'));
    const anchors = new Map();
    const counts = new Map();
    const headings = [];
    parser.walkTokens(tokens, token => {
      if (token.type !== 'heading') return;
      const base = slug(token.text);
      const count = counts.get(base) || 0;
      counts.set(base, count + 1);
      const originalId = base + (count ? `-${count}` : '');
      const root = headings.length === 0 && token.depth === 1;
      token.siteId = root ? id : `${id}--${originalId}`;
      token.siteDepth = Math.min(6, token.depth + 1);
      token.siteTitle = root ? title : null;
      anchors.set(originalId, token.siteId);
      headings.push({id: token.siteId, title: token.siteTitle || text(token.text), depth: token.siteDepth});
    });
    if (headings[0]?.depth !== 2) throw new Error(`Missing top-level title: ${source}`);
    documents.set(source, {source, id, title, tokens, anchors, headings});
  }

  const assets = new Map();
  const github = 'https://github.com/LeBenco/FileSpacer/blob/main/';
  function resolve(source, href, image = false) {
    if (/^(?:[a-z][a-z\d+.-]*:|\/\/)/i.test(href)) return href;
    const at = href.indexOf('#');
    const file = decodeURIComponent(at < 0 ? href : href.slice(0, at)).replaceAll('\\', '/');
    const fragment = at < 0 ? '' : decodeURIComponent(href.slice(at + 1));
    const target = file ? path.posix.normalize(path.posix.join(path.posix.dirname(source), file)) : source;
    if (target.startsWith('../') || path.posix.isAbsolute(target)) throw new Error(`Path outside repository: ${href}`);
    if (image) {
      const output = `assets/content/${target}`;
      assets.set(target, output);
      return output;
    }
    const doc = documents.get(target);
    if (!doc) return github + target.split('/').map(encodeURIComponent).join('/') + (fragment ? '#' + encodeURIComponent(fragment) : '');
    const anchor = fragment ? doc.anchors.get(fragment) : doc.id;
    if (!anchor) throw new Error(`Missing heading in ${source}: ${href}`);
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
        return `<h${token.siteDepth} id="${escape(token.siteId)}"><a href="#${escape(token.siteId)}" class="anchor-heading" aria-labelledby="${escape(token.siteId)}"><svg viewBox="0 0 16 16" aria-hidden="true"><use xlink:href="#svg-link"></use></svg></a>${title}</h${token.siteDepth}>\n`;
      },
    }});
    rendered.push(`<section aria-labelledby="${doc.id}" data-source="${doc.source}">\n${renderer.parser(doc.tokens)}</section>`);
  }
  const html = `<h1 class="fs-9">FileSpacer</h1>\n${rendered.join('\n<hr>\n')}`;
  const ids = [...html.matchAll(/\bid="([^"]+)"/g)].map(match => match[1]);
  const missing = [...html.matchAll(/<a\b[^>]*href="#([^"]+)"/g)].filter(match => !ids.includes(match[1]));
  if (new Set(ids).size !== ids.length || missing.length) throw new Error('Duplicate heading IDs or broken internal links');

  await fs.mkdir(destination, {recursive: true});
  await fs.copyFile(path.join(template, '_config.yml'), path.join(destination, '_config.yml'));
  await fs.cp(path.join(template, '_includes'), path.join(destination, '_includes'), {recursive: true});
  await fs.cp(path.join(template, 'assets'), path.join(destination, 'assets'), {recursive: true});
  const nav = path.join(destination, '_includes/components/site_nav.html');
  await fs.mkdir(path.dirname(nav), {recursive: true});
  await fs.writeFile(nav, `<nav aria-label="Main" id="site-nav" class="site-nav"><ul class="nav-list">${[...documents.values()].map(doc => navigation(doc.headings)).join('')}</ul></nav>\n`);
  await fs.writeFile(path.join(destination, 'index.html'), `---\nlayout: default\ntitle: FileSpacer\nhas_toc: false\n---\n${html}`);
  for (const [source, target] of assets) {
    await fs.mkdir(path.dirname(path.join(destination, target)), {recursive: true});
    await fs.copyFile(path.join(repository, source), path.join(destination, target));
  }
  return {documents: documents.size, headings: ids.length, images: assets.size, destination};
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  console.log(JSON.stringify(await generate(), null, 2));
}
