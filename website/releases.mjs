import assert from 'node:assert/strict';

// Only published stable releases supply dates. Keep the result deterministic for
// the documentation cache, independently of GitHub's response order.
export async function fetchReleaseDates(request = fetch, token = process.env.GH_TOKEN || process.env.GITHUB_TOKEN) {
  const dates = new Map();
  const headers = {Accept: 'application/vnd.github+json', 'X-GitHub-Api-Version': '2026-03-10', 'User-Agent': 'FileSpacer-documentation'};
  if (token) headers.Authorization = `Bearer ${token}`;
  for (let page = 1; ; page++) {
    const response = await request(`https://api.github.com/repos/LeBenco/FileSpacer/releases?per_page=100&page=${page}`, {
      headers, signal: AbortSignal.timeout(20000),
    });
    if (!response.ok) throw new Error(`GitHub releases lookup failed (${response.status}).`);
    const releases = await response.json();
    assert(Array.isArray(releases), 'GitHub releases must be a JSON array.');
    for (const release of releases) {
      if (release.draft || release.prerelease || !/^v\d+\.\d+\.\d+$/.test(release.tag_name)) continue;
      const version = release.tag_name.slice(1);
      const value = release.published_at;
      const date = new Date(value);
      if (typeof value !== 'string' || !/^\d{4}-\d{2}-\d{2}T/.test(value)
          || !Number.isFinite(date.getTime()) || date.toISOString().slice(0, 10) !== value.slice(0, 10)) {
        throw new Error(`Missing or invalid publication date for ${release.tag_name}.`);
      }
      if (dates.has(version)) throw new Error(`Duplicate GitHub release: ${release.tag_name}.`);
      dates.set(version, date.toISOString());
    }
    if (releases.length < 100) break;
  }
  return Object.fromEntries([...dates].sort(([a], [b]) => a.localeCompare(b, 'en')));
}

export function releaseHeading(title, locale, dates) {
  const version = title.match(/^(\d+\.\d+\.\d+)(?=\s|$)/)?.[1];
  if (!version) return null;
  if (!Object.hasOwn(dates, version)) return version;
  const date = new Intl.DateTimeFormat(locale, {
    year: 'numeric', month: '2-digit', day: '2-digit', timeZone: 'UTC',
  }).format(new Date(dates[version]));
  return `${version} — ${date}`;
}
