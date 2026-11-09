import fs from 'node:fs/promises';
import path from 'node:path';
import assert from 'node:assert/strict';
import {fileURLToPath} from 'node:url';
import {Lexer, walkTokens} from 'marked';
import {generate, headingSlug, prepareHeadingAnchors} from './generate.mjs';

const repository = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const excluded = new Set(['node_modules', 'vendor', '.bundle', '.sass-cache', 'build', '.git', '.claude']);
async function files(root, directory) {
  const result = [];
  for (const entry of await fs.readdir(path.join(root, directory), {withFileTypes: true})) {
    if (excluded.has(entry.name)) continue;
    const file = path.posix.join(directory, entry.name);
    if (entry.isDirectory()) result.push(...await files(root, file));
    else if (entry.isFile()) result.push(file);
  }
  return result;
}

function outline(content) {
  const tokens = prepareHeadingAnchors(Lexer.lex(content));
  const counts = new Map(), headings = [], groups = [];
  walkTokens(tokens, token => {
    if (token.type === 'heading') {
      const base = headingSlug(token.text), count = counts.get(base) || 0;
      counts.set(base, count + 1);
      const native = base + (count ? `-${count}` : '');
      headings.push({...token, native, anchor: token.docAnchor || native});
    }
    if (['html', 'paragraph'].includes(token.type)) {
      const ids = [...token.text.matchAll(/\b(?:id|name)=["']([^"']+)["']/g)].map(match => match[1]);
      if (ids.length) groups.push(ids);
    }
  });
  return {headings, groups};
}

function withAnchors(content, headings) {
  const newline = content.includes('\r\n') ? '\r\n' : '\n';
  let normalized = content.replaceAll('\r\n', '\n'), cursor = 0;
  const edits = [];
  for (const heading of headings) {
    const start = normalized.indexOf(heading.raw, cursor);
    assert(start >= cursor, `Cannot locate heading: ${heading.text}`);
    cursor = start + heading.raw.length;
    const marker = `<a id="${heading.anchor}"></a>`;
    if (heading.docAnchor) {
      const old = `<a id="${heading.docAnchor}"></a>`;
      const previous = normalized.lastIndexOf(old, start);
      assert(previous >= 0 && !normalized.slice(previous + old.length, start).trim(), `Anchor must immediately precede ${heading.text}`);
      if (old !== marker) edits.push({start: previous, end: previous + old.length, text: marker});
    } else edits.push({start, end: start, text: marker + '\n\n'});
  }
  for (const edit of edits.reverse()) normalized = normalized.slice(0, edit.start) + edit.text + normalized.slice(edit.end);
  return normalized.replaceAll('\n', newline);
}

const cornerText = {
  en: ['Square corners are recommended for title-bar and toolbar menus.', 'Square corners'],
  fr: ['Des coins carrés sont recommandés pour les menus de la barre de titre et de la barre d’outils.', 'Des coins carrés'],
  de: ['Für die Menüs der Titelleiste und der Symbolleiste werden Ecken ohne Abrundung empfohlen.', 'Ecken ohne Abrundung'],
  el: ['Συνιστώνται τετράγωνες γωνίες για τα μενού της γραμμής τίτλου και της γραμμής εργαλείων.', 'τετράγωνες γωνίες'],
  es: ['Se recomiendan esquinas rectas en los menús de la barra de título y de la barra de herramientas.', 'esquinas rectas'],
  it: ['Per i menu della barra del titolo e della barra degli strumenti sono consigliati angoli retti.', 'angoli retti'],
  nl: ['Rechte hoeken worden aanbevolen voor menu’s van de titelbalk en werkbalk.', 'Rechte hoeken'],
  pl: ['Dla menu paska tytułu i paska narzędzi zalecane są prostokątne narożniki.', 'prostokątne narożniki'],
  pt: ['São recomendados cantos retos para os menus da barra de título e da barra de ferramentas.', 'cantos retos'],
  uk: ['Для меню рядка заголовка й панелі інструментів рекомендовано прямі кути.', 'прямі кути'],
};

export async function normalizeDocumentation(root = repository) {
  const directories = (await fs.readdir(path.join(root, 'docs'), {withFileTypes: true}))
    .filter(entry => entry.isDirectory() && /^[a-z]{2}$/.test(entry.name)).map(entry => entry.name).sort();
  const docFiles = await files(root, 'docs');
  const markdown = ['README.md', 'CHANGELOG.md', 'THIRD_PARTY_NOTICES.md', 'BUILD.md', ...docFiles.filter(file => file.endsWith('.md'))];
  const contents = new Map(await Promise.all(markdown.map(async file => [file, await fs.readFile(path.join(root, file), 'utf8')])));
  const renames = new Map();
  for (const language of directories) {
    for (const file of docFiles.filter(file => file.startsWith(`docs/${language}/`))) {
      const base = path.posix.basename(file);
      const neutral = base.replace(new RegExp(`[-_.](?:${directories.join('|')})(?:[-_][A-Z]{2})?(?=\\.[^.]+$)`, 'i'), '');
      if (neutral !== base) renames.set(file, path.posix.join(path.posix.dirname(file), neutral));
    }
  }
  const targets = new Set();
  for (const [from, to] of renames) {
    assert(!targets.has(to), `Filename collision: ${to}`);
    assert(!docFiles.includes(to), `Destination already exists: ${to}`);
    assert(path.resolve(root, to).startsWith(path.resolve(root) + path.sep));
    targets.add(to);
  }
  const references = new Map(renames);
  for (const language of directories) references.set(`assets/content/docs/${language}/`, 'assets/content/docs/');
  for (const file of docFiles) {
    const language = file.match(/^docs\/([a-z]{2})\//)?.[1];
    if (!language || renames.has(file)) continue;
    const extension = path.posix.extname(file);
    references.set(file.slice(0, -extension.length) + '-' + language + extension, file);
  }
  const outlines = new Map([...contents].map(([file, content]) => [file, outline(content)]));
  const maps = new Map();
  for (const [file, document] of outlines) {
    const match = file.match(/^docs\/([a-z]{2})\/(.+)$/);
    const relative = match?.[2];
    const english = !match || match[1] === 'en' ? file : ({
      'README.md': 'README.md', 'CHANGELOG.md': 'CHANGELOG.md', 'THIRD_PARTY_NOTICES.md': 'THIRD_PARTY_NOTICES.md',
      'technical/README.md': 'docs/en/README.md', 'building/BUILD.md': 'BUILD.md',
    })[relative] || 'docs/en/' + relative;
    const reference = outlines.get(english);
    assert(reference, `Missing English reference: ${file} -> ${english}`);
    const englishIds = new Set([...reference.headings.map(heading => heading.anchor), ...reference.groups.flat()]);
    const sameStructure = document.headings.length === reference.headings.length;
    const anchors = new Map();
    for (const [index, heading] of document.headings.entries()) {
      const target = heading.docAnchor && reference.headings.find(other => other.anchor === heading.docAnchor)
        || (sameStructure || index === 0 ? reference.headings[index] : undefined);
      assert(target && target.depth === heading.depth, `Heading structure differs: ${file}: ${heading.text}`);
      assert(![...anchors.values()].includes(target.anchor), `Duplicate canonical heading: ${file}#${target.anchor}`);
      anchors.set(heading.native, target.anchor);
      if (heading.docAnchor) anchors.set(heading.docAnchor, target.anchor);
      heading.anchor = target.anchor;
    }
    for (const group of document.groups) for (const [index, id] of group.entries()) {
      if (anchors.has(id)) continue;
      const equivalent = englishIds.has(id) ? id : group.slice(index + 1).find(other => englishIds.has(other));
      anchors.set(id, equivalent || id);
    }
    maps.set(file, anchors);
    contents.set(file, withAnchors(contents.get(file), document.headings));
  }
  function replaceHref(source, href) {
    const remote = href.match(/^https:\/\/(?:github\.com\/LeBenco\/FileSpacer\/(?:blob|tree)\/main\/|raw\.githubusercontent\.com\/LeBenco\/FileSpacer\/main\/)/)?.[0];
    if (!remote && /^(?:[a-z][a-z\d+.-]*:|\/\/)/i.test(href)) return href;
    const local = remote ? href.slice(remote.length) : href;
    const at = local.indexOf('#');
    const location = at < 0 ? local : local.slice(0, at), fragment = at < 0 ? '' : local.slice(at + 1);
    const [name, query] = location.split('?', 2);
    const target = remote || name.startsWith('/') ? decodeURIComponent(name).replace(/^\//, '')
      : name ? path.posix.normalize(path.posix.join(path.posix.dirname(source), decodeURIComponent(name))) : source;
    const destination = renames.get(target) || target;
    const canonical = maps.get(target)?.get(decodeURIComponent(fragment)) || fragment;
    let output = name;
    if (destination !== target) output = remote ? destination : name.startsWith('/') ? '/' + destination
      : path.posix.relative(path.posix.dirname(source), destination);
    return (remote || '') + output + (query === undefined ? '' : '?' + query) + (at < 0 ? '' : '#' + canonical);
  }
  for (const [file, content] of contents) {
    const replacements = new Map();
    walkTokens(Lexer.lex(content), token => {
      if (token.type === 'link' || token.type === 'image') replacements.set(token.href, replaceHref(file, token.href));
      if (token.type === 'html') for (const match of token.text.matchAll(/\b(?:href|src)=["']([^"']+)["']/g)) {
        replacements.set(match[1], replaceHref(file, match[1]));
      }
    });
    let result = content;
    for (const [from, to] of replacements) if (from !== to) result = result.split(from).join(to);
    const language = file.match(/^docs\/([a-z]{2})\/user-guide\.md$/)?.[1];
    if (language) {
      const [sentence, phrase] = cornerText[language];
      const link = `[${phrase}](nilesoft-shell.md#recommended-menu-appearance)`;
      if (!result.includes(link)) {
        assert.equal(result.split(sentence).length, 2, `Square-corner sentence not found: ${file}`);
        result = result.replace(sentence, sentence.replace(phrase, link));
      }
      result = result.replace(/(\*\*[^*\r\n]+?)\s*\((?:…|\.\.\.)\)(\*\*)/g, '$1$2');
    }
    contents.set(file, result);
  }
  // Update repository paths in scripts, tests and provenance without rewriting
  // historical capture filenames that are not paths to the current assets.
  const related = [...await files(root, 'website'), ...await files(root, '.github'), ...await files(root, 'tests'), ...docFiles]
    .filter(file => /\.(?:md|json|mjs|js|yml|yaml|ps1|py|cs|cmd|sh)$/.test(file));
  for (const file of new Set([...markdown, ...related])) {
    let content = contents.has(file) ? contents.get(file) : await fs.readFile(path.join(root, file), 'utf8');
    for (const [from, to] of references) {
      for (const transform of [value => value, value => value.replaceAll('/', '\\/').replaceAll('.', '\\.'),
        value => value.replaceAll('/', '\\'), value => value.replaceAll('/', '\\\\')]) {
        content = content.split(transform(from)).join(transform(to));
      }
    }
    contents.set(file, content);
  }
  const changed = [];
  for (const [file, content] of contents) {
    const previous = await fs.readFile(path.join(root, file), 'utf8');
    if (previous === content) continue;
    await fs.writeFile(path.join(root, file), content);
    changed.push(file);
  }
  for (const [from, to] of renames) await fs.rename(path.join(root, from), path.join(root, to));
  return {changed, renamed: Object.fromEntries(renames)};
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  console.log(JSON.stringify(await normalizeDocumentation(), null, 2));
  await generate(repository, path.join(repository, 'build/normalized-docs-source'), {recordTranslations: []});
}
