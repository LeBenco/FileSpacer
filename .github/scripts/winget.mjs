// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import {createHash} from 'node:crypto';
import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';

const packageId = 'LeBenco.FileSpacer';
const repository = 'LeBenco/FileSpacer';
const projectUrl = `https://github.com/${repository}`;

export function releaseVersion(tag) {
  if (!/^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.test(tag) || tag !== tag.trim()) {
    throw new Error('WinGet requires a stable vX.Y.Z release tag.');
  }
  const version = tag.slice(1);
  if (version.split('.').some(part => Number(part) > 255)) {
    throw new Error('Release version components must fit in one byte.');
  }
  return version;
}

export function installerMetadata(release, tag, update, installer) {
  const version = releaseVersion(tag);
  if (release.draft || release.prerelease || release.tag_name !== tag) {
    throw new Error('Only a published stable release can be submitted.');
  }
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

export function manifests({version, url, hash}) {
  const common = `PackageIdentifier: ${packageId}\nPackageVersion: ${version}\n`;
  const footer = type => `ManifestType: ${type}\nManifestVersion: 1.9.0\n`;
  return {
    [`${packageId}.yaml`]: common + 'DefaultLocale: en-US\n' + footer('version'),
    [`${packageId}.locale.en-US.yaml`]: common + `PackageLocale: en-US
Publisher: LeBenco
PublisherUrl: ${projectUrl}
PublisherSupportUrl: ${projectUrl}/issues
PackageName: FileSpacer
PackageUrl: ${projectUrl}
License: GPL-3.0-only
LicenseUrl: ${projectUrl}/blob/v${version}/LICENSE
ShortDescription: A spatial file manager for Windows.
Moniker: filespacer
Tags:
  - file-manager
  - spatial
ReleaseNotesUrl: ${projectUrl}/releases/tag/v${version}
` + footer('defaultLocale'),
    [`${packageId}.installer.yaml`]: common + `InstallerType: nullsoft
MinimumOSVersion: 10.0.0.0
UpgradeBehavior: install
ProductCode: FileSpacer
AppsAndFeaturesEntries:
  - DisplayName: FileSpacer
    ProductCode: FileSpacer
Installers:
  - Architecture: x64
    Scope: user
    ElevationRequirement: elevationProhibited
    InstallerUrl: ${url}
    InstallerSha256: ${hash}
    InstallerSwitches:
      Silent: /S
      SilentWithProgress: /S
      Custom: /CurrentUser
      InstallLocation: /D=<INSTALLPATH>
  - Architecture: x64
    Scope: machine
    ElevationRequirement: elevationRequired
    InstallerUrl: ${url}
    InstallerSha256: ${hash}
    InstallerSwitches:
      Silent: /S
      SilentWithProgress: /S
      Custom: /AllUsers
      InstallLocation: /D=<INSTALLPATH>
` + footer('installer')
  };
}

export async function prepare(tag, output, request = fetch) {
  const version = releaseVersion(tag);
  const headers = {'Accept': 'application/vnd.github+json'};
  if (process.env.GH_TOKEN) headers.Authorization = `Bearer ${process.env.GH_TOKEN}`;
  async function api(endpoint, allowMissing = false) {
    const response = await request(`https://api.github.com/${endpoint}`, {headers});
    if (allowMissing && response.status === 404) return null;
    if (!response.ok) throw new Error(`GitHub API failed (${response.status}): ${endpoint}`);
    return response.json();
  }
  const release = await api(`repos/${repository}/releases/tags/${tag}`);
  // Check eligibility before treating an existing submission as successful.
  if (release.draft || release.prerelease || release.tag_name !== tag) {
    throw new Error('Only a published stable release can be submitted.');
  }
  const existing = await api(`repos/microsoft/winget-pkgs/contents/manifests/l/LeBenco/FileSpacer/${version}`, true);
  if (existing) return 'Already present in winget-pkgs.';
  const query = `repo:microsoft/winget-pkgs is:pr is:open in:title "${packageId}" "${version}"`;
  const pulls = await api(`search/issues?q=${encodeURIComponent(query)}&per_page=100`);
  if (pulls.incomplete_results) throw new Error('GitHub returned incomplete PR search results.');
  const versionPattern = new RegExp(`(?:^|\\s)${version.replaceAll('.', '\\.')}(?=\\s|$)`);
  const existingPull = pulls.items.find(item => item.title.includes(packageId) && versionPattern.test(item.title));
  if (existingPull) return `Already submitted: ${existingPull.html_url}`;
  async function download(name) {
    const asset = release.assets.find(item => item.name === name);
    const expected = `${projectUrl}/releases/download/${tag}/${name}`;
    if (!asset || asset.browser_download_url !== expected) throw new Error(`Missing release asset: ${name}`);
    // Release assets are public; never forward the API token to a download host.
    const response = await request(expected);
    if (!response.ok) throw new Error(`Release download failed (${response.status}): ${name}`);
    return Buffer.from(await response.arrayBuffer());
  }
  const installer = await download(`FileSpacer-${version}-install.exe`);
  const update = await download('FileSpacer-update.txt');
  const metadata = installerMetadata(release, tag, update.toString('utf8'), installer);
  await fs.mkdir(output, {recursive: true});
  for (const [name, content] of Object.entries(manifests(metadata))) {
    await fs.writeFile(path.join(output, name), content, {flag: 'wx'});
  }
  return null;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const output = path.resolve('build/winget');
  const skipped = await prepare(process.env.RELEASE_TAG, output);
  const message = skipped || `Prepared ${packageId} ${process.env.RELEASE_TAG} from verified release assets.`;
  console.log(message);
  if (process.env.GITHUB_OUTPUT) await fs.appendFile(process.env.GITHUB_OUTPUT, `submit=${!skipped}\n`);
  if (process.env.GITHUB_STEP_SUMMARY) await fs.appendFile(process.env.GITHUB_STEP_SUMMARY, `${message}\n`);
}
