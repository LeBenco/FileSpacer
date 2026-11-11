// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import fs from 'node:fs/promises';
import {spawnSync} from 'node:child_process';
import {pathToFileURL} from 'node:url';
import {repository, githubApi, releaseVersion, publishedRelease} from './release-assets.mjs';

const identity = {name: 'Benco', email: '76206340+LeBenco@users.noreply.github.com'};
const wingetRepo = 'repos/microsoft/winget-pkgs';
const packagePattern = /(?:^|\s)Benco\.FileSpacer(?=\s|$)/;

export function newer(a, b) {
  if (!a) return false;
  const left = releaseVersion(a).split('.').map(Number);
  if (!b) return true;
  const right = releaseVersion(b).split('.').map(Number);
  for (let i = 0; i < 3; i++) if (left[i] !== right[i]) return left[i] > right[i];
  return false;
}

export function emptyState(channel) {
  if (!['winget', 'chocolatey'].includes(channel)) throw new Error('Unknown distribution channel.');
  return {version: 1, channel, initialized: false, accepted: null, active: [], pending: null};
}

export function validateState(state, channel) {
  emptyState(channel);
  if (state?.version !== 1 || state.channel !== channel || typeof state.initialized !== 'boolean'
      || !Array.isArray(state.active) || !Object.hasOwn(state, 'accepted') || !Object.hasOwn(state, 'pending')) {
    throw new Error('Invalid distribution state; refusing to reset it.');
  }
  for (const tag of [state.accepted, state.pending]) if (tag !== null) releaseVersion(tag);
  for (const entry of state.active) {
    releaseVersion(entry.tag);
    if (!['uncertain', 'submitted'].includes(entry.phase)
        || (entry.pr !== undefined && (channel !== 'winget' || !Number.isSafeInteger(entry.pr) || entry.pr < 1))) {
      throw new Error('Invalid active submission.');
    }
  }
  return state;
}

// Each channel has its own orphan branch. Contents updates use the blob SHA as
// a compare-and-swap guard; a conflicting writer fails instead of losing state.
export function stateStore(channel, api) {
  const branch = `distribution-${emptyState(channel).channel}`;
  const base = `repos/${repository}`;
  let sha = null, previous = null;
  return {
    async load() {
      const ref = await api(`${base}/git/ref/heads/${branch}`, {allowMissing: true});
      if (!ref) return emptyState(channel);
      const file = await api(`${base}/contents/state.json?ref=${branch}`);
      if (file.type !== 'file' || file.encoding !== 'base64' || !file.sha) throw new Error('Invalid state file response.');
      const state = validateState(JSON.parse(Buffer.from(file.content, 'base64').toString('utf8')), channel);
      sha = file.sha;
      previous = JSON.stringify(state);
      return state;
    },
    async save(state) {
      validateState(state, channel);
      if (JSON.stringify(state) === previous) return;
      const content = JSON.stringify(state, null, 2) + '\n';
      const message = `Record ${channel} submission and latest pending release`;
      if (sha) {
        const result = await api(`${base}/contents/state.json`, {method: 'PUT', body: {
          branch, sha, message, content: Buffer.from(content).toString('base64'), author: identity, committer: identity
        }});
        sha = result.content.sha;
      } else {
        const tree = await api(`${base}/git/trees`, {method: 'POST', body: {
          tree: [{path: 'state.json', mode: '100644', type: 'blob', content}]
        }});
        const commit = await api(`${base}/git/commits`, {method: 'POST', body: {
          message, tree: tree.sha, parents: [], author: identity, committer: identity
        }});
        await api(`${base}/git/refs`, {method: 'POST', body: {ref: `refs/heads/${branch}`, sha: commit.sha}});
        sha = tree.tree?.find(entry => entry.path === 'state.json')?.sha;
        // Create-tree responses need not include the tree entries.
        if (!sha) sha = (await api(`${base}/contents/state.json?ref=${branch}`)).sha;
      }
      previous = JSON.stringify(state);
    }
  };
}

export async function openWingetSubmissions(api) {
  const active = [];
  const query = 'repo:microsoft/winget-pkgs is:pr is:open in:title "Benco.FileSpacer"';
  for (let page = 1; ; page++) {
    const result = await api(`search/issues?q=${encodeURIComponent(query)}&per_page=100&page=${page}`);
    if (result.incomplete_results || result.total_count > 1000 || !Array.isArray(result.items)) {
      throw new Error('Incomplete WinGet submission search; refusing to submit.');
    }
    for (const item of result.items.filter(item => packagePattern.test(item.title))) {
      const version = item.title.match(/\bversion\s+(\d+\.\d+\.\d+)(?=\s|$)/i)?.[1];
      if (!version || !Number.isSafeInteger(item.number)) throw new Error('Unrecognized open FileSpacer PR; inspect WinGet submissions.');
      releaseVersion(`v${version}`);
      active.push({tag: `v${version}`, pr: item.number, phase: 'submitted'});
    }
    if (result.items.length < 100) return active;
  }
}

export function chocolateySearch(version, approvedOnly, run = spawnSync) {
  releaseVersion(`v${version}`);
  const args = ['search', 'filespacer', '--exact', `--version=${version}`,
    '--source=https://community.chocolatey.org/api/v2/', '--limit-output', '--no-progress', '--ignore-http-cache'];
  if (approvedOnly) args.push('--approved-only');
  const result = run('choco.exe', args, {encoding: 'utf8', timeout: 90000, windowsHide: true});
  // Enhanced exit codes may report 2 for an empty search. Any other error,
  // warning or unexpected output is inconclusive, never proof of absence.
  if (result.error || ![0, 2].includes(result.status) || result.stderr?.trim()) {
    throw new Error(`Chocolatey status lookup failed (${result.status ?? result.error?.code}).`);
  }
  const lines = (result.stdout || '').trim().split(/\r?\n/).filter(Boolean);
  if (lines.some(line => line.toLowerCase() !== `filespacer|${version}`) || (result.status === 2 && lines.length)) {
    throw new Error('Unexpected Chocolatey search output; refusing to infer package status.');
  }
  return lines.length > 0;
}

export function channelRemote(channel, api, search = chocolateySearch) {
  emptyState(channel);
  return {
    async inspect(entry) {
      const version = releaseVersion(entry.tag);
      if (channel === 'chocolatey') {
        if (search(version, true)) return {status: 'accepted'};
        return {status: search(version, false) ? 'pending' : 'missing'};
      }
      if (entry.pr) {
        const pr = await api(`${wingetRepo}/pulls/${entry.pr}`);
        if (pr.merged_at) return {status: 'accepted'};
        if (pr.state === 'open') return {status: 'pending', pr: entry.pr};
        if (pr.state === 'closed') return {status: 'rejected', pr: entry.pr};
        throw new Error('Unexpected WinGet PR state.');
      }
      const files = await api(`${wingetRepo}/contents/manifests/b/Benco/FileSpacer/${version}`, {allowMissing: true});
      return {status: files ? 'accepted' : 'missing'};
    },
    async discover(initial) {
      if (channel === 'winget') return {active: await openWingetSubmissions(api), accepted: []};
      if (!initial) return {active: [], accepted: []};
      // Migration: exact-version lookups also find unlisted moderation entries.
      // Do not assume that an empty ordinary Chocolatey search means no uploads.
      const found = {active: [], accepted: []};
      for (let page = 1; ; page++) {
        const releases = await api(`repos/${repository}/releases?per_page=100&page=${page}`);
        if (!Array.isArray(releases)) throw new Error('Invalid release list.');
        for (const release of releases) {
          if (release.draft || release.prerelease || !/^v\d+\.\d+\.\d+$/.test(release.tag_name)) continue;
          const entry = {tag: release.tag_name, phase: 'submitted'};
          const status = await this.inspect(entry);
          if (status.status === 'accepted') found.accepted.push(entry.tag);
          else if (status.status === 'pending') found.active.push(entry);
        }
        if (releases.length < 100) return found;
      }
    }
  };
}

function accept(state, tag) {
  if (newer(tag, state.accepted)) state.accepted = tag;
}

export async function plan(state, store, remote, requested = '', retry = false) {
  validateState(state, state.channel);
  // Persist a new request BEFORE network lookups: an outage must not lose Y/Z.
  if (requested && newer(requested, state.accepted) && newer(requested, state.pending)
      && state.active.every(entry => newer(requested, entry.tag))) state.pending = requested;
  await store.save(state);
  const discovered = await remote.discover(!state.initialized);
  for (const tag of discovered.accepted) accept(state, tag);
  for (const entry of discovered.active) {
    const existing = state.active.find(item => item.tag === entry.tag && (!item.pr || item.pr === entry.pr));
    if (existing) Object.assign(existing, entry);
    else state.active.push(entry);
  }
  state.initialized = true;
  const unresolved = [], statuses = [];
  for (const entry of state.active) {
    const result = await remote.inspect(entry);
    if (result.status === 'accepted') accept(state, entry.tag);
    else { entry.status = result.status; unresolved.push(entry); statuses.push(result.status); }
  }
  state.active = unresolved;
  if (state.pending && (!newer(state.pending, state.accepted)
      || state.active.some(entry => !newer(state.pending, entry.tag)))) state.pending = null;
  await store.save(state);
  if (state.active.length) {
    if (retry && state.active.length === 1 && state.active[0].tag === requested
        && ['missing', 'rejected'].includes(statuses[0])) return requested;
    return '';
  }
  if (!state.pending) return '';
  const entry = {tag: state.pending, phase: 'submitted'};
  const result = await remote.inspect(entry);
  if (result.status === 'missing') return state.pending;
  if (result.status === 'accepted') accept(state, entry.tag);
  else state.active.push({...entry, ...(result.pr ? {pr: result.pr} : {})});
  state.pending = null;
  await store.save(state);
  return '';
}

export async function begin(state, store, remote, tag, retry = false, runId = '') {
  releaseVersion(tag);
  const retrying = retry && state.active.length === 1 && state.active[0].tag === tag;
  if ((!retrying && (state.active.length || state.pending !== tag)) || !state.initialized) {
    throw new Error('Submission no longer owns the queue.');
  }
  const found = await remote.discover(false);
  if (found.active.length) throw new Error('Another submission is awaiting acceptance.');
  const status = await remote.inspect(retrying ? state.active[0] : {tag});
  if (status.status !== 'missing' && !(retrying && status.status === 'rejected')) {
    throw new Error('Remote status changed; run the coordinator again before submitting.');
  }
  // Reserve BEFORE the external side effect. A lost response or cancelled run
  // stays blocked until reconciled, rather than blindly uploading a duplicate.
  state.active = [{tag, phase: 'uncertain', ...(runId ? {runId} : {})}];
  if (state.pending === tag) state.pending = null;
  await store.save(state);
}

export async function submitted(state, store, tag, log = '') {
  if (state.active.length !== 1 || state.active[0].tag !== tag) throw new Error('Missing submission reservation.');
  const entry = state.active[0];
  entry.phase = 'submitted';
  if (state.channel === 'winget') {
    const urls = [...new Set([...log.matchAll(/https:\/\/github\.com\/microsoft\/winget-pkgs\/pull\/(\d+)/g)].map(match => Number(match[1])))];
    if (urls.length > 1) throw new Error('Ambiguous WinGet PR output; the reservation is retained.');
    if (urls.length) entry.pr = urls[0];
  }
  await store.save(state);
}

export function summary(state, tag = '') {
  const active = state.active.map(entry => `${entry.tag} (${entry.phase}${entry.status ? `, ${entry.status}` : ''}${entry.pr ? `, https://github.com/microsoft/winget-pkgs/pull/${entry.pr}` : ''})`).join(', ') || 'none';
  return `### ${state.channel}\n\nAccepted: ${state.accepted || 'none'}\n\nAwaiting acceptance: ${active}\n\nLatest pending release: ${state.pending || 'none'}\n\n`
    + (tag ? `Selected for submission: ${tag}.\n` : 'Unaccepted, closed or uncertain submissions keep the queue blocked.\n');
}

async function main() {
  if (process.env.GITHUB_REPOSITORY !== repository) throw new Error('Distribution is restricted to the FileSpacer repository.');
  const channel = process.env.DISTRIBUTION_CHANNEL;
  const tag = process.env.RELEASE_TAG || '';
  if (tag) releaseVersion(tag);
  const api = githubApi();
  const store = stateStore(channel, api);
  const mode = process.argv[2];
  if (mode === 'plan' && tag) publishedRelease(await api(`repos/${repository}/releases/tags/${tag}`), tag);
  if (mode === 'plan' && process.env.DRY_RUN === 'true') {
    if (channel !== 'chocolatey' || !tag) throw new Error('Dry run requires a Chocolatey release tag.');
    await fs.appendFile(process.env.GITHUB_OUTPUT, `tag=${tag}\nsubmit=true\n`);
    await fs.appendFile(process.env.GITHUB_ENV, `RELEASE_TAG=${tag}\n`);
    return;
  }
  const state = await store.load();
  let selected = '';
  if (mode === 'plan') {
    selected = await plan(state, store, channelRemote(channel, api), tag, process.env.RETRY_SUBMISSION === 'true');
    if (state.active.some(entry => ['missing', 'rejected'].includes(entry.status))) {
      console.log('::warning::Distribution remains blocked: investigate the missing or closed submission before explicitly retrying its tag.');
    }
    await fs.appendFile(process.env.GITHUB_OUTPUT, `tag=${selected}\nsubmit=${!!selected}\n`);
    if (selected) await fs.appendFile(process.env.GITHUB_ENV, `RELEASE_TAG=${selected}\n`);
  } else if (mode === 'begin') {
    await begin(state, store, channelRemote(channel, api), tag, process.env.RETRY_SUBMISSION === 'true', process.env.GITHUB_RUN_ID);
  } else if (mode === 'submitted') {
    await submitted(state, store, tag, process.argv[3] ? await fs.readFile(process.argv[3], 'utf8') : '');
  } else if (mode !== 'report') throw new Error('Expected plan, begin, submitted or report.');
  const report = summary(state, selected);
  console.log(report);
  if (process.env.GITHUB_STEP_SUMMARY) await fs.appendFile(process.env.GITHUB_STEP_SUMMARY, report);
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  main().catch(error => { console.error(error.message); process.exitCode = 1; });
}
