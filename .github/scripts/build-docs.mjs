// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import fs from 'node:fs/promises';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {generate} from '../../website/generate.mjs';

const scripts = path.dirname(fileURLToPath(import.meta.url));
const repository = path.resolve(scripts, '../..');

async function files(directory, prefix = '') {
  const result = [];
  for (const entry of (await fs.readdir(directory, {withFileTypes: true})).sort((a, b) => a.name.localeCompare(b.name, 'en'))) {
    if (!prefix && ['node_modules', '.bundle', '.sass-cache', 'vendor'].includes(entry.name)) continue;
    const name = path.posix.join(prefix, entry.name);
    if (entry.isDirectory()) result.push(...await files(path.join(directory, entry.name), name));
    else if (entry.isFile()) result.push(name);
  }
  return result;
}

function buildPath(directory) {
  const resolved = path.resolve(repository, directory);
  if (!resolved.startsWith(path.join(repository, 'build') + path.sep)) throw new Error('Documentation output must be inside build/');
  return resolved;
}

async function digest(file, normalize = false) {
  let bytes = await fs.readFile(file);
  if (normalize && /\.(?:md|mjs|js|html|css|yml|json|txt|lock|svg)$|(?:^|[\\/])Gemfile$/.test(file)) {
    bytes = Buffer.from(bytes.toString('utf8').replace(/^\uFEFF/, '').replaceAll('\r\n', '\n'));
  }
  return createHash('sha256').update(bytes).digest('hex');
}

export async function sourceDigest(root = repository) {
  const hash = createHash('sha256');
  const inputs = ['README.md', 'CHANGELOG.md', 'THIRD_PARTY_NOTICES.md', 'docs/user-guide.md', 'docs/privacy-policy.md', 'src/res/FileSpacer.svg'];
  for (const directory of ['website', 'docs/images', '.github/scripts']) {
    inputs.push(...(await files(path.join(root, directory))).map(file => directory + '/' + file));
  }
  for (const file of inputs.sort()) hash.update(file + '\0' + await digest(path.join(root, file), true) + '\0');
  return hash.digest('hex');
}

// Keep the site's URLs intact online; resolve them relative to each local page offline.
export function localUrl(value, file, base, origin) {
  let url = value;
  if (url.startsWith(origin + '/')) url = url.slice(origin.length);
  if (url !== base && !url.startsWith(base + '/')) return value;
  const match = url.slice(base.length).match(/^([^?#]*)(.*)$/);
  let target = match[1].replace(/^\//, '');
  if (!target || target.endsWith('/')) target += 'index.html';
  return (path.posix.relative(path.posix.dirname(file), target) || 'index.html') + match[2];
}

export function offlineSearch(script, documents) {
  const start = script.indexOf('function initSearch() {');
  const indexStart = script.indexOf('      lunr.tokenizer.separator', start);
  const indexEnd = script.indexOf('      searchLoaded(index, docs);', indexStart);
  const end = script.indexOf('\nfunction searchLoaded(', indexEnd);
  if (start < 0 || indexStart < 0 || indexEnd < 0 || end < 0
      || !script.slice(start, indexStart).includes('JSON.parse(request.responseText)')) {
    throw new Error('The pinned theme search loader changed; review the offline adaptation');
  }
  const data = JSON.stringify(documents).replaceAll('<', '\\u003c').replaceAll('\u2028', '\\u2028').replaceAll('\u2029', '\\u2029');
  return script.slice(0, start) + 'function initSearch() {\n  var docs = ' + data + ';\n'
    + script.slice(indexStart, indexEnd + '      searchLoaded(index, docs);'.length) + '\n}\n' + script.slice(end);
}

export async function makeOffline(output, languages, config) {
  const base = config.match(/^baseurl:\s*(.*)$/m)[1].trim().replace(/\/$/, '');
  const origin = config.match(/^url:\s*(.*)$/m)[1].trim().replace(/\/$/, '');
  for (const file of await files(output)) {
    if (!/\.(html|css|js)$/.test(file)) continue;
    const full = path.join(output, file);
    let content = await fs.readFile(full, 'utf8');
    // JavaScript resource URLs are resolved against the document, not the script.
    const context = file.endsWith('.js') ? path.posix.join(file.split('/assets/')[0] === file ? '' : file.split('/assets/')[0], 'index.html') : file;
    content = content.replace(/(["'])([^"'\r\n]+)\1/g, (all, quote, url) => quote + localUrl(url, context, base, origin) + quote);
    if (file.endsWith('.css')) content = content.replace(/url\(([^'"\s)]+)\)/g, (_, url) => `url(${localUrl(url, file, base, origin)})`);
    await fs.writeFile(full, content);
  }
  for (const language of languages) {
    const page = path.posix.join(language.path, 'index.html');
    const js = path.join(output, language.path, 'assets/js/just-the-docs.js');
    const data = JSON.parse(await fs.readFile(path.join(output, language.path, 'assets/js/search-data.json'), 'utf8'));
    for (const document of Object.values(data)) document.url = localUrl(document.url, page, base, origin);
    await fs.writeFile(js, offlineSearch(await fs.readFile(js, 'utf8'), data));
  }
}

export async function installerInclude(output, names) {
  const escape = value => value.replaceAll('$', '$$').replaceAll('"', '$\\"').replaceAll('/', '\\');
  const lines = ['; Generated by .github/scripts/build-docs.mjs. Do not edit.', '!macro InstallDocumentation'];
  let current;
  for (const file of [...names].sort()) {
    const directory = path.posix.dirname(file) === '.' ? '' : path.posix.dirname(file);
    if (directory !== current) {
      lines.push(`  SetOutPath "$INSTDIR\\help${directory ? '\\' + escape(directory) : ''}"`);
      current = directory;
    }
    lines.push(`  File "\${FILESPACER_DOCS}\\${escape(file)}"`);
  }
  lines.push('!macroend', '', '!macro RemoveDocumentation');
  lines.push('  !insertmacro CleanInstalledDocumentation "Uninstall" uninstall_failed');
  lines.push('!macroend', '');
  await fs.writeFile(output + '.nsh', lines.join('\r\n'));
}

export async function buildDocs(target = 'web', output = buildPath(target === 'web' ? 'build/site' : 'build/offline-docs')) {
  if (!['web', 'offline'].includes(target)) throw new Error('Use --target web or --target offline');
  output = buildPath(output);
  const sourceHash = await sourceDigest();
  let cached;
  try {
    cached = JSON.parse(await fs.readFile(path.join(output, 'build-info.json'), 'utf8'));
    if (cached.target !== target || cached.sourceHash !== sourceHash) cached = null;
    if (cached) for (const [file, hash] of Object.entries(cached.files)) {
      if (file.includes('..') || path.isAbsolute(file) || await digest(path.join(output, file)) !== hash) { cached = null; break; }
    }
  } catch (error) {
    if (error.code !== 'ENOENT') throw error;
    cached = null;
  }
  if (cached) {
    if (target === 'offline') await installerInclude(output, [...Object.keys(cached.files), 'build-info.json']);
    console.log(`Documentation (${target}) is current: ${output}`);
    return cached;
  }
  const source = buildPath(`build/docs-source-${target}`);
  if (output === source || output.startsWith(source + path.sep) || source.startsWith(output + path.sep)) throw new Error('Source and output directories must be separate');
  const environment = {...process.env, BUNDLE_GEMFILE: path.join(scripts, 'Gemfile'), JEKYLL_ENV: 'production'};
  const available = spawnSync('ruby', ['-S', 'bundle', 'exec', 'jekyll', '--version'], {cwd: scripts, env: environment, stdio: 'pipe'});
  if (available.error || available.status !== 0) throw new Error('Generating documentation requires Ruby and bundle install in .github/scripts. Existing output was preserved.');
  await fs.rm(source, {recursive: true, force: true});
  const generated = await generate(repository, source);
  await fs.rm(output, {recursive: true, force: true});
  for (const language of generated.languages) {
    const result = spawnSync('ruby', ['-S', 'bundle', 'exec', 'jekyll', 'build', '--trace', '--source', path.join(source, language.source), '--destination', path.join(output, language.path)], {
      cwd: path.join(source, language.source), stdio: 'inherit', env: environment,
    });
    if (result.error || result.status !== 0) throw new Error(`Jekyll failed (${language.code}). Install Ruby and run bundle install in .github/scripts. ${result.error?.message || ''}`);
  }
  const languages = generated.languages.map(({code, path}) => ({code, path}));
  if (target === 'offline') await makeOffline(output, languages, await fs.readFile(path.join(repository, 'website/_config.yml'), 'utf8'));
  await fs.copyFile(path.join(repository, 'website/_layouts/UPSTREAM-LICENSE.txt'), path.join(output, 'JustTheDocs-LICENSE.txt'));
  const hashes = {};
  for (const file of await files(output)) hashes[file] = await digest(path.join(output, file));
  const manifest = {target, sourceHash, languages, files: hashes};
  await fs.writeFile(path.join(output, 'build-info.json'), JSON.stringify(manifest, null, 2) + '\n');
  if (target === 'offline') await installerInclude(output, [...Object.keys(hashes), 'build-info.json']);
  console.log(`Built documentation (${target}): ${languages.length} languages, ${Object.keys(hashes).length} files`);
  return manifest;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2);
  if (args.length !== 2 || args[0] !== '--target') throw new Error('Usage: node .github/scripts/build-docs.mjs --target web|offline');
  await buildDocs(args[1]);
}
