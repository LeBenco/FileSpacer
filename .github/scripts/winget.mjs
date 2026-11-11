// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import {repository, githubApi, publishedRelease, releaseVersion, downloadInstaller} from './release-assets.mjs';

const packageId = 'Benco.FileSpacer';
const projectUrl = `https://github.com/${repository}`;

export function manifests({version, url, hash}) {
  const common = `PackageIdentifier: ${packageId}\nPackageVersion: ${version}\n`;
  const header = type => `# yaml-language-server: $schema=https://aka.ms/winget-manifest.${type}.1.9.0.schema.json\n\n`;
  const footer = type => `ManifestType: ${type}\nManifestVersion: 1.9.0\n`;
  return {
    [`${packageId}.yaml`]: header('version') + common + 'DefaultLocale: en-US\n' + footer('version'),
    [`${packageId}.locale.en-US.yaml`]: header('defaultLocale') + common + `PackageLocale: en-US
Publisher: Benco
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
    [`${packageId}.installer.yaml`]: header('installer') + common + `InstallerType: nullsoft
MinimumOSVersion: 10.0.0.0
UpgradeBehavior: install
ProductCode: FileSpacer
AppsAndFeaturesEntries:
  - DisplayName: FileSpacer
    ProductCode: FileSpacer
Installers:
  - Architecture: x64
    Scope: user
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
  const api = githubApi(process.env.GH_TOKEN, request);
  const release = publishedRelease(await api(`repos/${repository}/releases/tags/${tag}`), tag);
  const existing = await api(`repos/microsoft/winget-pkgs/contents/manifests/b/Benco/FileSpacer/${version}`, {allowMissing: true});
  if (existing) return 'Already present in winget-pkgs.';
  const query = `repo:microsoft/winget-pkgs is:pr is:open in:title "${packageId}" "${version}"`;
  const pulls = await api(`search/issues?q=${encodeURIComponent(query)}&per_page=100`);
  if (pulls.incomplete_results) throw new Error('GitHub returned incomplete PR search results.');
  const packagePattern = new RegExp(`(?:^|\\s)${packageId.replaceAll('.', '\\.')}(?=\\s|$)`);
  const versionPattern = new RegExp(`(?:^|\\s)${version.replaceAll('.', '\\.')}(?=\\s|$)`);
  const existingPull = pulls.items.find(item => packagePattern.test(item.title) && versionPattern.test(item.title));
  if (existingPull) return `Already submitted: ${existingPull.html_url}`;
  const metadata = await downloadInstaller(release, tag, request);
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
