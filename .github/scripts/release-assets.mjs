// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import {createHash} from 'node:crypto';

export const repository = 'LeBenco/FileSpacer';
export const projectUrl = `https://github.com/${repository}`;

export function releaseVersion(tag) {
  if (typeof tag !== 'string' || !/^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.test(tag) || tag !== tag.trim()) {
    throw new Error('Expected a stable vX.Y.Z release tag.');
  }
  const version = tag.slice(1);
  if (version.split('.').some(part => Number(part) > 255)) throw new Error('Release version components must fit in one byte.');
  return version;
}

export function publishedRelease(release, tag) {
  releaseVersion(tag);
  if (release.draft || release.prerelease || release.tag_name !== tag) {
    throw new Error('Only a published stable release can be submitted.');
  }
  return release;
}

export function githubApi(token = process.env.GH_TOKEN, request = fetch) {
  return async (endpoint, {allowMissing = false, method = 'GET', body} = {}) => {
    const headers = {Accept: 'application/vnd.github+json'};
    if (token) headers.Authorization = `Bearer ${token}`;
    if (body !== undefined) headers['Content-Type'] = 'application/json';
    const response = await request(`https://api.github.com/${endpoint}`, {
      method, headers, signal: AbortSignal.timeout(60000),
      ...(body === undefined ? {} : {body: JSON.stringify(body)})
    });
    if (allowMissing && response.status === 404) return null;
    if (!response.ok) throw new Error(`GitHub API failed (${response.status}): ${endpoint}`);
    return response.json();
  };
}

export function installerMetadata(release, tag, update, installer) {
  const version = releaseVersion(tag);
  publishedRelease(release, tag);
  const name = `FileSpacer-${version}-install.exe`;
  const url = `${projectUrl}/releases/download/${tag}/${name}`;
  const asset = release.assets.find(item => item.name === name);
  if (!asset || asset.browser_download_url !== url || asset.size !== installer.length) {
    throw new Error('The published installer is missing or its size/URL does not match.');
  }
  const hash = createHash('sha256').update(installer).digest('hex');
  const updateVersion = [...version.split('.'), '0']
    .map(part => Number(part).toString(16).padStart(2, '0').toUpperCase()).join('');
  const expected = ['FSUP', updateVersion, `${projectUrl}/releases/tag/${tag}`, url, hash, String(installer.length)];
  if (update.trimEnd().replaceAll('\r', '') !== expected.join('\n')) {
    throw new Error('The installer does not match FileSpacer-update.txt.');
  }
  if (asset.digest && asset.digest.toLowerCase() !== `sha256:${hash}`) {
    throw new Error('The installer does not match the GitHub asset digest.');
  }
  return {version, url, hash: hash.toUpperCase()};
}

export async function downloadInstaller(release, tag, request = fetch) {
  const version = releaseVersion(tag);
  publishedRelease(release, tag);
  async function download(name) {
    const url = `${projectUrl}/releases/download/${tag}/${name}`;
    if (!release.assets.some(asset => asset.name === name && asset.browser_download_url === url)) {
      throw new Error(`Missing official release asset: ${name}`);
    }
    // Release assets are public: never forward the API credential to download hosts.
    const response = await request(url);
    if (!response.ok) throw new Error(`Release download failed (${response.status}): ${name}`);
    return Buffer.from(await response.arrayBuffer());
  }
  const installer = await download(`FileSpacer-${version}-install.exe`);
  const update = await download('FileSpacer-update.txt');
  return installerMetadata(release, tag, update.toString('utf8'), installer);
}
