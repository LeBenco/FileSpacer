// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import {repository, githubApi, publishedRelease, releaseVersion, downloadInstaller} from './release-assets.mjs';

const templates = new URL('../../installer/chocolatey/', import.meta.url);

export async function packageFiles(metadata) {
  const replacements = {'@VERSION@': metadata.version, '@INSTALLER_URL@': metadata.url,
    '@INSTALLER_SHA256@': metadata.hash};
  const files = {};
  for (const name of ['filespacer.nuspec', 'tools/chocolateyinstall.ps1', 'tools/chocolateyuninstall.ps1']) {
    let content = await fs.readFile(new URL(name, templates), 'utf8');
    for (const [key, value] of Object.entries(replacements)) content = content.replaceAll(key, value);
    files[name] = content;
  }
  return files;
}

export async function prepare(tag, output, request = fetch) {
  releaseVersion(tag);
  const release = publishedRelease(await githubApi(process.env.GH_TOKEN, request)(`repos/${repository}/releases/tags/${tag}`), tag);
  const metadata = await downloadInstaller(release, tag, request);
  for (const [name, content] of Object.entries(await packageFiles(metadata))) {
    const target = path.join(output, name);
    await fs.mkdir(path.dirname(target), {recursive: true});
    await fs.writeFile(target, content, {flag: 'wx'});
  }
  return metadata;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const metadata = await prepare(process.env.RELEASE_TAG, path.resolve('build/chocolatey/filespacer'));
  if (process.env.GITHUB_OUTPUT) await fs.appendFile(process.env.GITHUB_OUTPUT, `version=${metadata.version}\n`);
  console.log(`Prepared filespacer ${metadata.version} from the verified published installer (${metadata.hash}).`);
}
