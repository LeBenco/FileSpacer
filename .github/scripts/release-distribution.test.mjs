// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import {execFileSync, spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {distributionFromMessage, publicationForTag, selectDistribution} from './release-distribution.mjs';

const root = fileURLToPath(new URL('../../', import.meta.url));
await fs.mkdir(path.join(root, 'build'), {recursive: true});
const repository = 'LeBenco/FileSpacer';
const bash = process.platform === 'win32' ? 'C:/Program Files/Git/bin/bash.exe' : 'bash';
const quote = value => `'${value.replaceAll("'", "'\\''")}'`;
function git(cwd, ...args) {
  return execFileSync(bash, ['-lc', `git ${args.map(quote).join(' ')}`], {cwd, encoding: 'utf8', stdio: ['pipe', 'pipe', 'pipe']}).trim();
}

test('tag annotations default to none and select an explicit cumulative set', () => {
  assert.deepEqual(distributionFromMessage('FileSpacer 1.3.3\n'), []);
  assert.deepEqual(distributionFromMessage('FileSpacer-Distribution: none\n'), []);
  assert.deepEqual(distributionFromMessage('FileSpacer-Distribution: winget\n'), ['winget']);
  assert.deepEqual(distributionFromMessage('FileSpacer 1.3.3\r\n\r\nFileSpacer-Distribution: chocolatey,winget,chocolatey\r\n'), ['chocolatey', 'winget']);
  for (const value of ['', 'both', 'all', 'scoop', 'none,winget', 'winget,', 'winget\nFileSpacer-Distribution: chocolatey']) {
    assert.throws(() => distributionFromMessage(`FileSpacer-Distribution: ${value}\n`));
  }
});

test('automatic jobs respect only selected channels; manual jobs can submit a GitHub-only release', () => {
  const publication = publicationForTag('FileSpacer-Distribution: winget', 'v1.3.3', repository);
  assert.deepEqual(selectDistribution(publication, 'winget', repository), {tag: 'v1.3.3', enabled: true});
  assert.equal(selectDistribution(publication, 'chocolatey', repository).enabled, false);
  const legacy = {tag: 'v1.3.2', repository};
  assert.equal(selectDistribution(legacy, 'winget', repository).enabled, false);
  assert.equal(selectDistribution(legacy, 'chocolatey', repository).enabled, false);
  assert.deepEqual(selectDistribution(null, 'chocolatey', repository, 'v1.3.2'), {tag: 'v1.3.2', enabled: true});
  for (const invalid of [{...publication, repository: 'someone/else'}, {...publication, distribution: 'winget'},
    {...publication, distribution: ['none']}, {...publication, tag: 'v1.3.3\n'}, {...publication, tag: 'v1.3.3-rc1'}]) {
    assert.throws(() => selectDistribution(invalid, 'winget', repository));
  }
  assert.throws(() => selectDistribution(null, 'winget', repository, 'v1.3.2\n'));
});

test('the workflow CLI records the tag selection and exposes explicit manual selection without an artifact', async t => {
  const directory = await fs.mkdtemp(path.join(root, 'build/distribution-cli-'));
  t.after(() => fs.rm(directory, {recursive: true, force: true}));
  const message = path.join(directory, 'tag.txt'), publication = path.join(directory, 'publication.json');
  const output = path.join(directory, 'output');
  await fs.writeFile(message, 'FileSpacer 1.3.3\n\nFileSpacer-Distribution: winget,chocolatey\n');
  const script = fileURLToPath(new URL('release-distribution.mjs', import.meta.url));
  const env = {...process.env, RELEASE_TAG: 'v1.3.3', GITHUB_REPOSITORY: repository, GITHUB_OUTPUT: output};
  execFileSync(process.execPath, [script, 'record', message, publication], {env});
  assert.deepEqual(JSON.parse(await fs.readFile(publication, 'utf8')).distribution, ['winget', 'chocolatey']);
  execFileSync(process.execPath, [script, 'select', publication], {env: {...env, GITHUB_EVENT_NAME: 'workflow_run', DISTRIBUTION_CHANNEL: 'winget'}});
  assert.equal(await fs.readFile(output, 'utf8'), 'tag=v1.3.3\nenabled=true\n');
  await fs.writeFile(output, '');
  execFileSync(process.execPath, [script, 'select', path.join(directory, 'missing.json')], {env: {...env, GITHUB_EVENT_NAME: 'workflow_dispatch', DISTRIBUTION_CHANNEL: 'chocolatey'}});
  assert.equal(await fs.readFile(output, 'utf8'), 'tag=v1.3.3\nenabled=true\n');
});

test('release.sh rejects invalid selections before touching Git', async t => {
  const directory = await fs.mkdtemp(path.join(root, 'build/distribution-args-'));
  t.after(() => fs.rm(directory, {recursive: true, force: true}));
  const script = path.join(root, 'release.sh').replaceAll('\\', '/');
  for (const args of [['--distribute'], ['--distribute', 'both'], ['--distribute', 'none', '--distribute', 'winget'],
    ['--distribute', 'chocolatey', '--distribute', 'none'], ['--unknown']]) {
    const result = spawnSync(bash, ['-lc', `bash ${[script, '1.3.3', ...args].map(quote).join(' ')}`], {cwd: directory, encoding: 'utf8'});
    assert.equal(result.status, 1, result.stderr);
    assert.doesNotMatch(result.stderr, /not a git repository/);
  }
});

test('release.sh pushes only local fixture tags with the previewed channel annotation', async t => {
  const cases = [[[], []], [['none'], []], [['winget'], ['winget']], [['chocolatey'], ['chocolatey']],
    [['winget', 'chocolatey', 'winget'], ['winget', 'chocolatey']]];
  for (const [channels, expected] of cases) {
    await t.test(channels.join(',') || 'default none', async t => {
      const directory = await fs.mkdtemp(path.join(root, 'build/distribution-git-'));
      t.after(() => fs.rm(directory, {recursive: true, force: true}));
      const remote = path.join(directory, 'remote.git').replaceAll('\\', '/');
      const client = path.join(directory, 'client');
      git(directory, 'init', '--bare', '--initial-branch=main', remote);
      git(directory, 'init', '--initial-branch=main', client.replaceAll('\\', '/'));
      git(client, 'config', 'user.name', 'Benco');
      git(client, 'config', 'user.email', '76206340+LeBenco@users.noreply.github.com');
      await fs.copyFile(path.join(root, 'release.sh'), path.join(client, 'release.sh'));
      await fs.writeFile(path.join(client, 'CHANGELOG.md'), '# Changelog\n\n## 1.3.3 — 2026-11-05\n\n- Fixture release.\n');
      git(client, 'add', '.');
      git(client, 'commit', '-m', 'Fixture');
      git(client, 'remote', 'add', 'origin', remote);
      git(client, 'push', '-u', 'origin', 'main');
      const args = channels.flatMap(channel => ['--distribute', channel]);
      const result = execFileSync(bash, ['release.sh', '1.3.3', ...args], {cwd: client, input: 'y\n', encoding: 'utf8', stdio: ['pipe', 'pipe', 'pipe']});
      assert.match(result, new RegExp(`Distribution: ${expected.join(',') || 'none'}`));
      const annotation = git(directory, '--git-dir=' + remote, 'for-each-ref', '--format=%(contents)', 'refs/tags/v1.3.3');
      const publication = publicationForTag(annotation, 'v1.3.3', repository);
      assert.deepEqual(publication.distribution, expected);
      for (const channel of ['winget', 'chocolatey']) {
        assert.equal(selectDistribution(publication, channel, repository).enabled, expected.includes(channel));
      }
    });
  }
});
