// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {fileURLToPath, pathToFileURL} from 'node:url';
import {emptyState, validateState, newer, plan, begin, submitted, stateStore,
  chocolateySearch, channelRemote, openWingetSubmissions} from './distribution-queue.mjs';
import {githubApi} from './release-assets.mjs';

const X = 'v1.2.3', Y = 'v1.2.4', Z = 'v1.2.5';
function fixture(channel) {
  let saved = {...emptyState(channel), initialized: true};
  const versions = new Map(), writes = [];
  const store = {
    async load() { return structuredClone(saved); },
    async save(state) { saved = structuredClone(state); writes.push(structuredClone(state)); }
  };
  const remote = {
    async inspect(entry) { return {status: versions.get(entry.tag) || 'missing'}; },
    async discover() { return {active: [], accepted: []}; }
  };
  async function select(tag = '', retry = false) { return plan(await store.load(), store, remote, tag, retry); }
  async function send(tag) {
    await begin(await store.load(), store, remote, tag);
    versions.set(tag, 'pending');
    await submitted(await store.load(), store, tag);
  }
  return {store, remote, versions, writes, select, send};
}

for (const channel of ['winget', 'chocolatey']) {
  test(`${channel}: X accepted -> submit Y; hold Z until Y is accepted`, async () => {
    const f = fixture(channel);
    assert.equal(await f.select(X), X);
    await f.send(X);
    assert.equal(await f.select(Y), '');
    assert.equal((await f.store.load()).pending, Y);
    f.versions.set(X, 'accepted');
    assert.equal(await f.select(), Y);
    await f.send(Y);
    assert.equal(await f.select(Z), '');
    assert.equal((await f.store.load()).pending, Z);
    f.versions.set(Y, 'accepted');
    assert.equal(await f.select(), Z);
  });

  test(`${channel}: Z replaces pending Y; only Z is selected after X acceptance`, async () => {
    const f = fixture(channel);
    await f.select(X); await f.send(X);
    assert.equal(await f.select(Y), '');
    assert.equal(await f.select(Z), '');
    // A delayed/replayed workflow for Y must not roll the queue back.
    assert.equal(await f.select(Y), '');
    assert.equal((await f.store.load()).pending, Z);
    f.versions.set(X, 'accepted');
    assert.equal(await f.select(), Z);
    await f.send(Z);
    f.versions.set(Z, 'accepted');
    assert.equal(await f.select(Y), '');
    assert.equal((await f.store.load()).accepted, Z);
    assert.equal((await f.store.load()).pending, null);
  });

  test(`${channel}: persist the latest request even when the status service fails`, async () => {
    const f = fixture(channel);
    f.remote.discover = async () => { throw new Error('service unavailable'); };
    await assert.rejects(f.select(Y), /service unavailable/);
    await assert.rejects(f.select(Z), /service unavailable/);
    assert.equal((await f.store.load()).pending, Z);
  });

  test(`${channel}: cancellation/lost response never resubmits automatically; explicit same-tag retry preserves Z`, async () => {
    const f = fixture(channel);
    await f.select(X);
    await begin(await f.store.load(), f.store, f.remote, X);
    assert.equal((await f.store.load()).active[0].phase, 'uncertain');
    assert.equal(await f.select(Z), '');
    assert.equal(await f.select(), '');
    assert.equal(await f.select(Z, true), '');
    assert.equal(await f.select(X, true), X);
    await begin(await f.store.load(), f.store, f.remote, X, true);
    f.versions.set(X, 'pending');
    await submitted(await f.store.load(), f.store, X);
    assert.equal(await f.select(X, true), ''); // It actually arrived; do not retry.
    assert.equal((await f.store.load()).pending, Z);
    f.versions.set(X, 'accepted');
    assert.equal(await f.select(), Z);
  });

  test(`${channel}: preparation failures keep the candidate; rejection blocks newer releases`, async () => {
    const f = fixture(channel);
    assert.equal(await f.select(X), X);
    assert.equal(await f.select(), X); // No reservation or upload took place.
    await f.send(X);
    f.versions.set(X, 'rejected');
    assert.equal(await f.select(Z), '');
    assert.equal(await f.select(), '');
    assert.equal((await f.store.load()).active[0].status, 'rejected');
    assert.equal(await f.select(X, true), X);
  });
}

test('channels are independent and multiple legacy submissions must ALL be accepted', async () => {
  const w = fixture('winget'), c = fixture('chocolatey');
  await w.store.save({...emptyState('winget'), active: [{tag: X, phase: 'submitted'}, {tag: Y, phase: 'submitted'}]});
  w.versions.set(X, 'pending'); w.versions.set(Y, 'accepted');
  assert.equal(await w.select(Z), '');
  assert.equal(await c.select(Z), Z);
  w.versions.set(X, 'accepted');
  assert.equal(await w.select(), Z);
});

test('a legacy/external submission is adopted and a lost successful response is reconciled', async () => {
  const f = fixture('winget');
  f.remote.discover = async () => ({active: [{tag: X, pr: 12, phase: 'submitted'}], accepted: []});
  f.versions.set(X, 'pending');
  assert.equal(await f.select(Z), '');
  assert.equal((await f.store.load()).active[0].pr, 12);
  f.remote.discover = async () => ({active: [], accepted: []});
  f.versions.set(X, 'accepted');
  assert.equal(await f.select(), Z);
  await begin(await f.store.load(), f.store, f.remote, Z);
  // The worker died after uploading, before recording success.
  f.versions.set(Z, 'pending');
  assert.equal(await f.select(), '');
  f.versions.set(Z, 'accepted');
  assert.equal(await f.select(), '');
});

test('begin rechecks remote state and refuses another PR, a duplicate or a stale candidate', async () => {
  const f = fixture('winget');
  await f.select(X);
  await assert.rejects(begin(await f.store.load(), f.store, f.remote, Y), /owns the queue/);
  f.versions.set(X, 'pending');
  await assert.rejects(begin(await f.store.load(), f.store, f.remote, X), /Remote status changed/);
  f.remote.discover = async () => ({active: [{tag: Y, pr: 10, phase: 'submitted'}], accepted: []});
  await assert.rejects(begin(await f.store.load(), f.store, f.remote, X), /Another submission/);
  assert.equal((await f.store.load()).pending, X);
});

test('invalid persisted state and unsupported versions are never silently reset', () => {
  for (const state of [{}, {...emptyState('winget'), version: 2}, {...emptyState('winget'), channel: 'chocolatey'},
    {...emptyState('winget'), pending: 'v1.2.3\n'}, {...emptyState('winget'), active: [{tag: X, phase: 'unknown'}]},
    {...emptyState('winget'), active: [{tag: X, phase: 'submitted', pr: '12'}]}]) {
    assert.throws(() => validateState(state, 'winget'));
  }
  assert.equal(newer('v1.10.0', 'v1.9.99'), true);
  assert.equal(newer(X, X), false);
  assert.throws(() => newer('v256.0.0', null));
});

test('state uses isolated orphan branches, author identity, blob-SHA updates and fails on conflicts', async () => {
  let file = null, exists = false, writes = 0;
  const calls = [];
  const api = async (endpoint, options = {}) => {
    calls.push({endpoint, options});
    if (endpoint.includes('/git/ref/')) return exists ? {object: {sha: 'commit'}} : null;
    if (endpoint.includes('/contents/')) {
      if (options.method === 'PUT') {
        assert.equal(options.body.branch, 'distribution-winget');
        assert.equal(options.body.sha, file.sha);
        assert.equal(options.body.author.name, 'Benco');
        assert.deepEqual(options.body.author, options.body.committer);
        if (writes++) throw new Error('409 conflict');
        file = {...file, content: options.body.content, sha: 'updated-blob'};
        return {content: {sha: file.sha}};
      }
      return file;
    }
    if (endpoint.endsWith('/git/trees')) {
      assert.equal(options.body.tree.length, 1);
      const entry = options.body.tree[0];
      assert.equal(entry.path, 'state.json');
      file = {type: 'file', encoding: 'base64', content: Buffer.from(entry.content).toString('base64'), sha: 'initial-blob'};
      return {sha: 'tree'};
    }
    if (endpoint.endsWith('/git/commits')) {
      assert.deepEqual(options.body.parents, []);
      assert.equal(options.body.author.email, '76206340+LeBenco@users.noreply.github.com');
      return {sha: 'commit'};
    }
    if (endpoint.endsWith('/git/refs')) { exists = true; return {}; }
    assert.fail(endpoint);
  };
  const store = stateStore('winget', api);
  const state = await store.load();
  state.pending = X; await store.save(state);
  const reopened = stateStore('winget', api);
  assert.deepEqual(await reopened.load(), state);
  state.pending = Y; await reopened.save(state);
  const count = calls.length;
  await reopened.save(state);
  assert.equal(calls.length, count, 'unchanged state must not create commits');
  state.pending = Z;
  await assert.rejects(reopened.save(state), /409/);
  assert.equal(JSON.parse(Buffer.from(file.content, 'base64').toString()).pending, Y);
  await assert.rejects(stateStore('winget', async endpoint => endpoint.includes('/git/ref/') ? {} : null).load());
});

test('WinGet search paginates, matches the exact publisher and refuses incomplete results', async () => {
  const item = n => ({number: n, title: `New version: Benco.FileSpacer version 1.2.${n}`});
  const entries = await openWingetSubmissions(async endpoint => {
    assert.ok(decodeURIComponent(endpoint).includes('is:open'));
    return endpoint.endsWith('page=1')
      ? {items: Array.from({length: 100}, (_, n) => item(n + 1)), total_count: 102}
      : {items: [item(101), {number: 200, title: 'New version: LeBenco.FileSpacer version 1.2.3'}], total_count: 102};
  });
  assert.equal(entries.length, 101);
  for (const result of [{items: [], incomplete_results: true}, {items: [], total_count: 1001},
    {items: [{number: 1, title: 'Update Benco.FileSpacer unknown version'}]}]) {
    await assert.rejects(openWingetSubmissions(async () => result));
  }
});

test('WinGet acceptance requires fusion; a closed PR is not acceptance', async () => {
  for (const [pr, expected] of [[{state: 'closed', merged_at: '2026-11-11'}, 'accepted'],
    [{state: 'closed', merged_at: null}, 'rejected'], [{state: 'open'}, 'pending']]) {
    const remote = channelRemote('winget', async endpoint => { assert.ok(endpoint.endsWith('/pulls/123')); return pr; });
    assert.equal((await remote.inspect({tag: X, pr: 123})).status, expected);
  }
  const remote = channelRemote('winget', async endpoint => { assert.ok(endpoint.endsWith('/1.2.3')); return null; });
  assert.equal((await remote.inspect({tag: X})).status, 'missing');
});

test('successful submission retains the PR reference and ambiguous output keeps the persisted reservation', async () => {
  const f = fixture('winget');
  await f.select(X); await begin(await f.store.load(), f.store, f.remote, X);
  await assert.rejects(submitted(await f.store.load(), f.store, X,
    'https://github.com/microsoft/winget-pkgs/pull/1 https://github.com/microsoft/winget-pkgs/pull/2'), /Ambiguous/);
  assert.equal((await f.store.load()).active[0].phase, 'uncertain');
  await submitted(await f.store.load(), f.store, X, 'Submitted: https://github.com/microsoft/winget-pkgs/pull/123');
  assert.equal((await f.store.load()).active[0].pr, 123);
});

test('Chocolatey uses official exact-version searches and approval is distinct from mere presence', async () => {
  for (const approved of [false, true]) {
    const present = await chocolateySearch('1.2.3', approved, (command, args, options) => {
      assert.equal(command, 'choco.exe');
      assert.ok(args.includes('--version=1.2.3') && args.includes('--exact') && args.includes('--limit-output'));
      assert.equal(args.includes('--approved-only'), approved);
      assert.equal(options.timeout, 90000);
      return {status: 0, stdout: 'filespacer|1.2.3\r\n', stderr: ''};
    });
    assert.equal(present, true);
  }
  for (const status of [0, 2]) assert.equal(await chocolateySearch('1.2.3', true, () => ({status, stdout: ''})), false);
  for (const result of [{status: 1}, {status: null, error: {code: 'ETIMEDOUT'}},
    {status: 0, stdout: 'service unavailable'}, {status: 0, stdout: 'filespacer|1.2.4'},
    {status: 0, stderr: 'network error'}, {status: 2, stdout: 'filespacer|1.2.3'}]) {
    await assert.rejects(chocolateySearch('1.2.3', true, () => result));
  }
  const remote = channelRemote('chocolatey', null, (_version, approvedOnly) => !approvedOnly);
  assert.equal((await remote.inspect({tag: X})).status, 'pending');
});

test('Chocolatey waits for asynchronous status results before declaring approval or availability', async () => {
  const calls = [];
  const remote = channelRemote('chocolatey', null, async (version, approvedOnly) => {
    await new Promise(resolve => setImmediate(resolve));
    calls.push({version, approvedOnly});
    return !approvedOnly;
  });
  assert.equal((await remote.inspect({tag: X})).status, 'pending');
  assert.deepEqual(calls, [{version: '1.2.3', approvedOnly: true}, {version: '1.2.3', approvedOnly: false}]);
  await assert.rejects(channelRemote('chocolatey', null, async () => {
    throw new Error('lookup failed');
  }).inspect({tag: X}), /lookup failed/);
});

test('Chocolatey migration checks published versions individually, imports moderation and runs only once', async () => {
  const calls = [];
  const remote = channelRemote('chocolatey', async () => [
    {tag_name: X}, {tag_name: Y}, {tag_name: Z, prerelease: true}, {tag_name: 'v1.2.6', draft: true}
  ], (version, approved) => {
    calls.push({version, approved});
    return version === '1.2.3' || !approved;
  });
  const initial = await remote.discover(true);
  assert.deepEqual(initial.accepted, [X]);
  assert.deepEqual(initial.active.map(entry => entry.tag), [Y]);
  assert.equal(calls.length, 3);
  assert.deepEqual(await remote.discover(false), {active: [], accepted: []});
  assert.equal(calls.length, 3);
});

test('GitHub requests authenticate only the API and reject rate limits and bad state responses', async () => {
  const api = githubApi('fixture-token', async (url, options) => {
    assert.ok(url.startsWith('https://api.github.com/'));
    assert.equal(options.headers.Authorization, 'Bearer fixture-token');
    assert.ok(options.signal);
    return new Response('', {status: 403});
  });
  await assert.rejects(api('repos/LeBenco/FileSpacer/git/ref/heads/distribution-winget', {allowMissing: true}), /403/);
  const unavailable = githubApi('fixture-token', async () => {
    throw new TypeError('fetch failed', {cause: {code: 'ENETUNREACH'}});
  });
  await assert.rejects(unavailable('repos/LeBenco/FileSpacer/releases?per_page=100'),
    error => error.message.includes('GET repos/LeBenco/FileSpacer/releases?per_page=100')
      && error.message.includes('ENETUNREACH') && !error.message.includes('fixture-token'));
});

test('dry run validates its release but never reads or writes queue state', async t => {
  const root = fileURLToPath(new URL('../../', import.meta.url));
  await fs.mkdir(path.join(root, 'build'), {recursive: true});
  const directory = await fs.mkdtemp(path.join(root, 'build/queue-dry-run-'));
  t.after(() => fs.rm(directory, {recursive: true, force: true}));
  const preload = path.join(directory, 'http.mjs');
  await fs.writeFile(preload, `globalThis.fetch = async url => {
    if (!url.endsWith('/releases/tags/${X}')) throw new Error('Dry run touched state');
    return Response.json({tag_name: '${X}', draft: false, prerelease: false});
  };`);
  const output = path.join(directory, 'output'), envFile = path.join(directory, 'env');
  await fs.writeFile(output, ''); await fs.writeFile(envFile, '');
  execFileSync(process.execPath, ['--import', pathToFileURL(preload).href, '.github/scripts/distribution-queue.mjs', 'plan'], {
    cwd: root, env: {...process.env, GITHUB_REPOSITORY: 'LeBenco/FileSpacer', DISTRIBUTION_CHANNEL: 'chocolatey',
      RELEASE_TAG: X, DRY_RUN: 'true', GITHUB_OUTPUT: output, GITHUB_ENV: envFile}
  });
  assert.equal(await fs.readFile(output, 'utf8'), `tag=${X}\nsubmit=true\n`);
  assert.equal(await fs.readFile(envFile, 'utf8'), `RELEASE_TAG=${X}\n`);
});
