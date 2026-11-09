// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import test from 'node:test';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {packageFiles, prepare} from './chocolatey.mjs';

const root = 'https://github.com/LeBenco/FileSpacer';
const installer = Buffer.from('fixture, not an executable');
const hash = createHash('sha256').update(installer).digest('hex');
const url = `${root}/releases/download/v1.3.2/FileSpacer-1.3.2-install.exe`;
const update = `FSUP\n01030200\n${root}/releases/tag/v1.3.2\n${url}\n${hash}\n${installer.length}\n`;
const release = {tag_name: 'v1.3.2', draft: false, prerelease: false, assets: [
  {name: 'FileSpacer-1.3.2-install.exe', browser_download_url: url, size: installer.length, digest: `sha256:${hash}`},
  {name: 'FileSpacer-update.txt', browser_download_url: `${root}/releases/download/v1.3.2/FileSpacer-update.txt`}
]};

test('package pins the official installer and checksum with native machine-wide arguments', async () => {
  const files = await packageFiles({version: '1.3.2', url, hash: hash.toUpperCase()});
  assert.equal(Object.keys(files).length, 3);
  const spec = files['filespacer.nuspec'];
  const install = files['tools/chocolateyinstall.ps1'];
  assert.ok(spec.includes('<id>filespacer</id>') && spec.includes('<version>1.3.2</version>'));
  assert.ok(spec.includes('/blob/v1.3.2/LICENSE'));
  assert.ok(spec.includes('<copyright>Copyright (c) 2026 Benco</copyright>'));
  assert.ok(spec.includes('each folder has a single window that remembers its position, size and view settings'));
  assert.ok(spec.includes('https://raw.githubusercontent.com/LeBenco/FileSpacer/v1.3.2/docs/en/images/spatial-windows.png'));
  await fs.access(new URL('../../docs/en/images/spatial-windows.png', import.meta.url));
  assert.ok(install.includes(url) && install.includes(hash.toUpperCase()));
  assert.ok(install.includes("silentArgs = '/S /AllUsers'"));
  assert.ok(install.includes("checksumType64 = 'sha256'"));
  assert.ok(install.includes('Get-OSArchitectureWidth'));
  assert.ok(!install.includes('/DefaultFileManager'));
  assert.ok(!Object.values(files).some(content => /@(VERSION|INSTALLER_URL|INSTALLER_SHA256)@/.test(content)));
});

test('generation verifies published installer bytes without forwarding API credentials', async t => {
  const output = await fs.mkdtemp(path.join(os.tmpdir(), 'filespacer-chocolatey-'));
  t.after(() => fs.rm(output, {recursive: true, force: true}));
  const request = async (address, options) => {
    if (address.includes('/releases/tags/')) return Response.json(release);
    assert.equal(options, undefined);
    return new Response(address === url ? installer : update);
  };
  assert.equal((await prepare('v1.3.2', output, request)).hash, hash.toUpperCase());
  assert.equal((await fs.readdir(output)).length, 2);
  assert.ok((await fs.readFile(path.join(output, 'tools/chocolateyinstall.ps1'), 'utf8')).includes(url));
});

test('unpublished releases, wrong assets and inconsistent update checksums cannot produce a package', async t => {
  const output = await fs.mkdtemp(path.join(os.tmpdir(), 'filespacer-chocolatey-invalid-'));
  t.after(() => fs.rm(output, {recursive: true, force: true}));
  for (const invalid of [{...release, draft: true}, {...release, prerelease: true}, {...release, assets: []}]) {
    await assert.rejects(prepare('v1.3.2', output, async () => Response.json(invalid)));
  }
  await assert.rejects(prepare('v1.3.2', output, async address => address.includes('/releases/tags/')
    ? Response.json(release) : new Response(address === url ? installer : update.replace(hash, '0'.repeat(64)))));
  assert.deepEqual(await fs.readdir(output), []);
  await assert.rejects(prepare('v1.3.2', output, async () => new Response('', {status: 403})), /403/);
});

test('PowerShell package helpers reject x86 and uninstall only the machine installation', async t => {
  const directory = await fs.mkdtemp(path.join(os.tmpdir(), 'filespacer chocolatey helpers '));
  t.after(() => fs.rm(directory, {recursive: true, force: true}));
  await fs.writeFile(path.join(directory, 'uninstall.exe'), 'fixture; never executed');
  const quote = text => `'${text.replaceAll("'", "''")}'`;
  const templates = fileURLToPath(new URL('../../installer/chocolatey/tools/', import.meta.url));
  const script = `
$ErrorActionPreference = 'Stop'
$script:width = 64
$script:installCalls = 0
$script:uninstallCalls = 0
$directory = ${quote(directory)}
function Get-OSArchitectureWidth { return $script:width }
function Install-ChocolateyPackage {
    param($PackageName, $FileType, $Url64bit, $Checksum64, $ChecksumType64, $SilentArgs, $ValidExitCodes)
    if ($SilentArgs -cne '/S /AllUsers' -or $ChecksumType64 -cne 'sha256' -or $ValidExitCodes.Count -ne 1 -or $ValidExitCodes[0] -ne 0) { throw 'Invalid install arguments' }
    $script:installCalls++
}
function Get-UninstallRegistryKey { param($SoftwareName) return $script:keys }
function Uninstall-ChocolateyPackage {
    param($PackageName, $FileType, $File, $SilentArgs, $ValidExitCodes)
    if ($File -cne (Join-Path $directory 'uninstall.exe') -or $SilentArgs -cne "/S /AllUsers _?=$directory" -or $ValidExitCodes.Count -ne 1 -or $ValidExitCodes[0] -ne 0) { throw 'Invalid uninstall arguments' }
    $script:uninstallCalls++
}
. ${quote(path.join(templates, 'chocolateyinstall.ps1'))}
if ($script:installCalls -ne 1) { throw 'Install helper was not called' }
$script:width = 32
$rejected = $false
try { . ${quote(path.join(templates, 'chocolateyinstall.ps1'))} } catch { $rejected = $_.Exception.Message -eq 'FileSpacer requires Windows x64.' }
if (!$rejected -or $script:installCalls -ne 1) { throw 'x86 was not rejected before installation' }
$personal = [pscustomobject]@{PSPath='Microsoft.PowerShell.Core\\Registry::HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FileSpacer';PSChildName='FileSpacer';InstallLocation=$directory}
$machine = [pscustomobject]@{PSPath='Microsoft.PowerShell.Core\\Registry::HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FileSpacer';PSChildName='FileSpacer';InstallLocation=$directory}
$script:keys = @($personal)
. ${quote(path.join(templates, 'chocolateyuninstall.ps1'))}
if ($script:uninstallCalls -ne 0) { throw 'Personal installation must be preserved' }
$script:keys = @($personal, $machine)
. ${quote(path.join(templates, 'chocolateyuninstall.ps1'))}
if ($script:uninstallCalls -ne 1) { throw 'Machine uninstaller was not called once' }
if (Test-Path -LiteralPath (Join-Path $directory 'uninstall.exe')) { throw 'In-place uninstaller was not removed after exit' }
if (!(Test-Path -LiteralPath $directory)) { throw 'A non-empty installation directory must be preserved' }
$script:keys = @($machine, $machine)
$rejected = $false
try { . ${quote(path.join(templates, 'chocolateyuninstall.ps1'))} } catch { $rejected = $true }
if (!$rejected -or $script:uninstallCalls -ne 1) { throw 'Ambiguous installations must be rejected' }
`;
  const filename = path.join(directory, 'helpers.ps1');
  await fs.writeFile(filename, script);
  execFileSync('powershell.exe', ['-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', filename], {encoding: 'utf8'});
});
