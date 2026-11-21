import fs from 'node:fs/promises';
import path from 'node:path';
import {optimize} from 'svgo';

// A request the flag services did not answer, as opposed to an answer that fails validation.
class FlagRequestError extends Error {}

// Download flag artwork at build time; generated pages use local optimized files only.
export async function downloadFlags(countries, request = fetch) {
  const codes = [...new Set(countries)];
  if (!codes.length) return new Map();
  if (codes.some(code => !/^[a-z]{2}$/.test(code))) throw new Error('Invalid flag country code');
  async function text(url) {
    let response;
    try {
      response = await request(url, {
        headers: {'User-Agent': 'FileSpacer-Documentation/1.0 (https://github.com/LeBenco/FileSpacer)'},
        signal: AbortSignal.timeout(20000),
      });
      if (response.ok) return await response.text();
    } catch (error) {
      throw new FlagRequestError(`Flag download failed (${error.message}): ${url}`, {cause: error});
    }
    throw new FlagRequestError(`Flag download failed (${response.status}): ${url}`);
  }
  async function json(url) {
    const data = JSON.parse(await text(url));
    if (data.error) throw new Error(`Flag metadata error: ${data.error.info || data.error.code}`);
    return data;
  }
  // Truthy properties select the preferred flag, excluding deprecated statements.
  // No country-to-file table or country-specific artwork corrections are needed.
  const query = `PREFIX wdt: <http://www.wikidata.org/prop/direct/>
SELECT DISTINCT ?code ?flag WHERE {
  VALUES ?code { ${codes.map(code => `"${code.toUpperCase()}"`).join(' ')} }
  ?country wdt:P297 ?code; wdt:P41 ?flag.
  FILTER NOT EXISTS { ?country wdt:P576 ?end }
}`;
  const endpoint = new URL('https://query.wikidata.org/sparql');
  endpoint.search = new URLSearchParams({format: 'json', query});
  const bindings = (await json(endpoint.href)).results?.bindings;
  if (!Array.isArray(bindings)) throw new Error('Invalid Wikidata flag response');
  const titles = new Map(codes.map(code => {
    const matches = [...new Set(bindings.filter(row => row.code?.value === code.toUpperCase()).map(row => row.flag?.value))];
    if (matches.length !== 1 || !/^https?:\/\/commons\.wikimedia\.org\/wiki\/Special:FilePath\//.test(matches[0])) {
      throw new Error(`Expected one current Wikimedia flag for ${code}`);
    }
    return [code, 'File:' + decodeURIComponent(matches[0].split('/Special:FilePath/')[1]).replaceAll('_', ' ')];
  }));
  const images = new Map();
  // Commons accepts at most 50 file titles per anonymous request.
  const files = [...new Set(titles.values())];
  for (let start = 0; start < files.length; start += 50) {
    const api = new URL('https://commons.wikimedia.org/w/api.php');
    api.search = new URLSearchParams({action: 'query', format: 'json', redirects: '1', prop: 'imageinfo',
      iiprop: 'url|extmetadata', titles: files.slice(start, start + 50).join('|')});
    const data = await json(api.href);
    const pages = new Map(Object.values(data.query?.pages || {}).map(page => [page.title, page.imageinfo?.[0]]));
    const aliases = new Map([...(data.query?.normalized || []), ...(data.query?.redirects || [])].map(alias => [alias.from, alias.to]));
    for (const title of files.slice(start, start + 50)) {
      let resolved = title;
      const seen = new Set();
      while (aliases.has(resolved) && !seen.has(resolved)) {
        seen.add(resolved);
        resolved = aliases.get(resolved);
      }
      const image = pages.get(resolved), metadata = image?.extmetadata;
      if (!image?.url) throw new Error(`Missing Wikimedia flag file: ${title}`);
      if (!/^(?:Public domain|CC0)$/.test(metadata?.LicenseShortName?.value)
          || metadata?.AttributionRequired?.value !== 'false') {
        throw new Error(`Flag requires a public-domain or CC0 source without required attribution: ${title}`);
      }
      const url = new URL(image.url);
      if (url.origin !== 'https://upload.wikimedia.org' || !url.pathname.endsWith('.svg')) {
        throw new Error(`Invalid Wikimedia flag URL: ${image.url}`);
      }
      url.search = '';
      images.set(title, url.href);
    }
  }
  const results = await Promise.allSettled(codes.map(async code => {
    const url = images.get(titles.get(code));
    const svg = await text(url);
    if (!/^\s*(?:<\?xml[^]*?\?>\s*)?(?:<!--[^]*?-->\s*|<!DOCTYPE svg[^>]*>\s*)*<svg\b[^]*<\/svg>\s*$/.test(svg)) {
      throw new Error(`Invalid flag SVG: ${url}`);
    }
    return [code, optimize(svg, {multipass: true}).data];
  }));
  // A parallel request failure must not hide a response that fails validation.
  const failures = results.filter(result => result.status === 'rejected');
  const failure = failures.find(result => !(result.reason instanceof FlagRequestError)) || failures[0];
  if (failure) throw failure.reason;
  return new Map(results.map(result => result.value));
}

function notice(message) {
  console.warn(process.env.GITHUB_ACTIONS ? `::warning::${message}` : `WARNING: ${message}`);
}

// Keep the last successful download beside the build, for the builds made while
// the flag services do not answer. Only an unanswered request falls back: an
// answer that fails validation is never replaced by an older copy.
export async function cachedFlags(countries, directory, request = fetch, warn = notice) {
  const file = code => path.join(directory, `${code}.svg`);
  try {
    const flags = await downloadFlags(countries, request);
    await fs.mkdir(directory, {recursive: true});
    for (const [code, svg] of flags) await fs.writeFile(file(code), svg);
    return flags;
  } catch (error) {
    if (!(error instanceof FlagRequestError)) throw error;
    let kept;
    try {
      kept = await Promise.all([...new Set(countries)].map(async code => [code, await fs.readFile(file(code), 'utf8')]));
    } catch {
      throw error;
    }
    warn(`Flag services unavailable, using the last downloaded flags. ${error.message}`);
    return new Map(kept);
  }
}
