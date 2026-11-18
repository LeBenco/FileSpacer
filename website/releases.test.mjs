import test from 'node:test';
import assert from 'node:assert/strict';
import {fetchReleaseDates, releaseHeading} from './releases.mjs';

const release = (tag_name, published_at, extra = {}) => ({tag_name, published_at, draft: false, prerelease: false, ...extra});

test('release dates come from paginated GitHub JSON, excluding drafts and prereleases', async () => {
  const first = [release('v1.3.2', '2026-11-05T23:59:59Z'), release('v1.3.1', '2026-11-03T18:49:58Z'),
    ...Array.from({length: 98}, () => release('draft', null, {draft: true}))];
  const urls = [];
  const dates = await fetchReleaseDates(async (url, options) => {
    urls.push(url);
    assert.equal(options.headers.Authorization, 'Bearer fixture-token');
    assert.equal(options.headers.Accept, 'application/vnd.github+json');
    return Response.json(urls.length === 1 ? first : [
      release('v1.3.0', '2026-11-02T14:50:53Z'),
      release('v1.4.0', null, {prerelease: true}), release('nightly', null),
    ]);
  }, 'fixture-token');
  assert.equal(urls.length, 2);
  assert.ok(urls[0].endsWith('per_page=100&page=1'));
  assert.ok(urls[1].endsWith('per_page=100&page=2'));
  assert.deepEqual(dates, {
    '1.3.0': '2026-11-02T14:50:53.000Z',
    '1.3.1': '2026-11-03T18:49:58.000Z',
    '1.3.2': '2026-11-05T23:59:59.000Z',
  });
  assert.deepEqual(await fetchReleaseDates(async () => Response.json(first.slice(0, 2).reverse()), ''),
    Object.fromEntries(Object.entries(dates).slice(1)));
});

test('failed lookups and invalid metadata cannot silently reuse dates from Markdown', async () => {
  await assert.rejects(fetchReleaseDates(async () => new Response('', {status: 403}), ''), /lookup failed \(403\)/);
  await assert.rejects(fetchReleaseDates(async () => {throw new Error('Network unavailable');}, ''), /Network unavailable/);
  for (const value of [null, 'invalid', '2026-02-30T12:00:00Z']) {
    await assert.rejects(fetchReleaseDates(async () => Response.json([release('v1.3.2', value)]), ''), /invalid publication date/);
  }
  await assert.rejects(fetchReleaseDates(async () => Response.json({message: 'invalid'}), ''), /JSON array/);
  const entry = release('v1.3.2', '2026-11-04T22:29:08Z');
  await assert.rejects(fetchReleaseDates(async () => Response.json([entry, entry]), ''), /Duplicate GitHub release/);
});

test('numeric dates use Intl locale conventions and the UTC publication day, regardless of source heading text', () => {
  const dates = {'1.3.2': '2026-11-05T23:59:59.000Z'};
  for (const [locale, expected] of [['fr-FR', '05/11/2026'], ['en-GB', '05/11/2026'], ['de-DE', '05.11.2026'], ['nl-NL', '05-11-2026']]) {
    assert.equal(releaseHeading('1.3.2 — an arbitrary original title', locale, dates), `1.3.2 — ${expected}`);
    assert.equal(releaseHeading('1.3.2', locale, dates), `1.3.2 — ${expected}`);
  }
  assert.equal(releaseHeading('1.4.0 — 2026-12-01', 'fr-FR', dates), '1.4.0');
  assert.equal(releaseHeading('Unreleased', 'fr-FR', dates), null);
});
