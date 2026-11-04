// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import fs from 'node:fs/promises';
import {pathToFileURL} from 'node:url';

export const evidencePath = 'build/ci/regression-pass.json';

export function applicationInput(file) {
  return /^(src|third_party|tests)\//.test(file)
    || ['.gitattributes', 'Makefile', 'build.cmd', 'tests.cmd', 'generate-icon.py', 'installer/installer.mak',
      '.github/workflows/regression.yml'].includes(file)
    || file.startsWith('.github/scripts/regression-cache');
}

export function fingerprint(tree, category = 'All', pattern = '*') {
  const entries = tree.split('\0').filter(Boolean).filter(entry => {
    const separator = entry.indexOf('\t');
    if (separator < 0) throw new Error('Invalid Git tree entry.');
    return applicationInput(entry.slice(separator + 1));
  }).sort();
  if (!entries.some(entry => entry.includes('\tsrc/'))) throw new Error('No application sources in the selected revision.');
  return createHash('sha256').update(JSON.stringify({version: 1, runner: 'windows-2022', scope: 'Application', category, pattern, entries})).digest('hex');
}

export function cacheKey(hash) {
  return `regression-pass-v1-${hash}`;
}

export function reusable(record, hash, repository, now = Date.now()) {
  const created = Date.parse(record?.createdAt);
  return record?.version === 1 && record.conclusion === 'success'
    && record.fingerprint === hash && record.repository === repository
    && /^[0-9a-f]{40}$/.test(record.executedSha ?? '') && /^\d+$/.test(String(record.runId ?? ''))
    && Number.isFinite(created) && created <= now;
}

async function output(name, value) {
  if (process.env.GITHUB_OUTPUT) await fs.appendFile(process.env.GITHUB_OUTPUT, `${name}=${value}\n`);
}

async function main() {
  if (process.argv[2] === 'fingerprint') {
    const tree = execFileSync('git', ['ls-tree', '-rz', '--full-tree', 'HEAD'], {encoding: 'utf8'});
    const hash = fingerprint(tree, process.env.TEST_CATEGORY, process.env.TEST_PATTERN);
    await output('fingerprint', hash);
    await output('key', cacheKey(hash));
    console.log(`Application/test inputs: ${hash}`);
  } else if (process.argv[2] === 'validate') {
    let record;
    try { record = JSON.parse((await fs.readFile(evidencePath, 'utf8')).replace(/^\uFEFF/, '')); }
    catch { /* Missing or malformed evidence requires a new execution. */ }
    const reuse = reusable(record, process.env.TEST_FINGERPRINT, process.env.GITHUB_REPOSITORY);
    await output('reuse', String(reuse));
    if (reuse) {
      const url = `${process.env.GITHUB_SERVER_URL}/${process.env.GITHUB_REPOSITORY}/actions/runs/${record.runId}`;
      const summary = `Regression PASS reused from [run ${record.runId}](${url}), executed at ${record.executedSha}. `
        + `The selected commit ${process.env.TEST_SHA} has identical application, build and test inputs for this selection.\n`;
      console.log(summary);
      if (process.env.GITHUB_STEP_SUMMARY) await fs.appendFile(process.env.GITHUB_STEP_SUMMARY, summary);
    }
  } else throw new Error('Expected fingerprint or validate.');
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  main().catch(error => { console.error(error.message); process.exitCode = 1; });
}
