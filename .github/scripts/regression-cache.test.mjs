// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import test from 'node:test';
import assert from 'node:assert/strict';
import {applicationInput, fingerprint, cacheKey, reusable} from './regression-cache.mjs';

const entry = (file, hash = 'a'.repeat(40)) => `100644 blob ${hash}\t${file}\0`;
const base = entry('src/main.cpp') + entry('tests/regression/cases/example.ps1');

test('documentation, screenshots and NSIS-only changes preserve the tested application identity', () => {
  const original = fingerprint(base);
  for (const file of ['README.md', 'docs/en/privacy-policy.md', 'docs/pl/images/overview-pl.png',
    'website/generate.mjs', 'installer/install.nsi', 'installer/generate-notes.mjs', '.github/workflows/winget.yml']) {
    assert.equal(applicationInput(file), false, file);
    assert.equal(fingerprint(base + entry(file, 'b'.repeat(40))), original, file);
  }
});

test('application code, resources, dependencies, build inputs and the test harness invalidate reuse', () => {
  for (const file of ['src/main.cpp', 'src/resource.h', 'src/resource.pl.rc', 'src/res/FileSpacer.svg',
    'third_party/sqlite/sqlite3.c', '.gitattributes', 'Makefile', 'build.cmd', 'tests.cmd', 'generate-icon.py',
    'installer/installer.mak', 'tests/regression/common/EssentialUi.cs', '.github/workflows/regression.yml',
    '.github/scripts/regression-cache.mjs']) {
    assert.equal(applicationInput(file), true, file);
    assert.notEqual(fingerprint(base + entry(file, 'b'.repeat(40))), fingerprint(base), file);
  }
});

test('different selections cannot reuse a partial test result', () => {
  assert.notEqual(fingerprint(base, 'All', '*'), fingerprint(base, 'Internal', '*'));
  assert.notEqual(fingerprint(base, 'All', '*'), fingerprint(base, 'All', '31-*'));
  assert.equal(fingerprint(base), fingerprint(base.split('\0').filter(Boolean).reverse().join('\0') + '\0'));
  assert.throws(() => fingerprint(entry('README.md')), /No application sources/);
});

test('only matching successful evidence from this repository is reusable', () => {
  const now = Date.parse('2026-11-04T12:00:00Z');
  const hash = fingerprint(base);
  const record = {version: 1, conclusion: 'success', fingerprint: hash, repository: 'Benco/FileSpacer',
    executedSha: 'a'.repeat(40), runId: '123', createdAt: new Date(now - 1000).toISOString()};
  assert.equal(reusable(record, hash, record.repository, now), true);
  for (const change of [{version: 0}, {conclusion: 'failure'}, {fingerprint: 'other'}, {repository: 'other/repo'},
    {executedSha: ''}, {runId: ''}, {createdAt: new Date(now + 1000).toISOString()}, {createdAt: 'invalid'}]) {
    assert.equal(reusable({...record, ...change}, hash, record.repository, now), false);
  }
  assert.equal(reusable(undefined, hash, record.repository, now), false);
  assert.equal(cacheKey(hash), `regression-pass-v1-${hash}`);
});
