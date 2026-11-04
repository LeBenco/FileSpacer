import fs from 'node:fs/promises';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {Lexer, walkTokens} from 'marked';

const decode = value => value.replace(/&amp;/g, '&').replace(/&quot;/g, '"').replace(/&#39;|&apos;/g, "'");
const plain = tokens => (tokens ?? []).map(token => token.tokens ? plain(token.tokens)
  : token.type === 'html' ? token.text.replace(/<[^>]*>/g, '') : token.text ?? '').join('');

export async function checkLinks(repository, tracked) {
  const paths = new Set(tracked);
  for (const file of tracked) {
    for (let dir = path.posix.dirname(file); dir !== '.'; dir = path.posix.dirname(dir)) paths.add(dir);
  }
  const documents = tracked.filter(file => file.endsWith('.md'));
  const anchors = new Map(), references = [];
  for (const file of documents) {
    const ids = new Set(), headings = new Set();
    walkTokens(Lexer.lex(await fs.readFile(path.join(repository, file), 'utf8')), token => {
      if (token.type === 'heading') {
        const base = decode(plain(token.tokens)).toLowerCase().replace(/[^\p{L}\p{M}\p{N}_\-\s]/gu, '').replace(/\s/g, '-');
        let id = base, count = 0;
        while (headings.has(id)) id = `${base}-${++count}`;
        headings.add(id);
        ids.add(id);
      }
      if (token.type === 'link' || token.type === 'image') references.push({file, href: decode(token.href)});
      if (token.type === 'html') {
        for (const match of token.text.matchAll(/\b(?:id|name)=["']([^"']+)["']/g)) ids.add(decode(match[1]));
        for (const match of token.text.matchAll(/\b(?:href|src)=["']([^"']+)["']/g)) references.push({file, href: decode(match[1])});
      }
    });
    anchors.set(file, ids);
  }
  const failures = [];
  let checked = 0;
  for (const reference of references) {
    let href = reference.href;
    for (const base of ['https://github.com/LeBenco/FileSpacer/blob/main/',
      'https://github.com/LeBenco/FileSpacer/tree/main/', 'https://raw.githubusercontent.com/LeBenco/FileSpacer/main/']) {
      if (href.startsWith(base)) href = '/' + href.slice(base.length);
    }
    if (/^[a-z]+:|^\/\//i.test(href)) continue;
    const [location, fragment] = href.split('#', 2);
    const pathname = decodeURIComponent(location.split('?')[0]);
    const target = pathname ? (pathname.startsWith('/') ? pathname.slice(1)
      : path.posix.normalize(path.posix.join(path.posix.dirname(reference.file), pathname))) : reference.file;
    checked++;
    if (!paths.has(target)) failures.push({...reference, target, reason: 'Missing tracked path (case-sensitive)'});
    else if (fragment && anchors.has(target) && !anchors.get(target).has(decodeURIComponent(fragment))) {
      failures.push({...reference, target, reason: 'Missing section anchor'});
    }
  }
  return {documents: documents.length, checked, failures};
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const repository = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
  const tracked = execFileSync('git', ['ls-files', '-z'], {cwd: repository, encoding: 'utf8'}).split('\0').filter(Boolean);
  const result = await checkLinks(repository, tracked);
  console.log(JSON.stringify(result, null, 2));
  if (result.failures.length) process.exitCode = 1;
}
