// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import fs from 'node:fs/promises';
import {pathToFileURL} from 'node:url';
import {releaseVersion} from './release-assets.mjs';

const channels = ['winget', 'chocolatey'];

export function distributionFromMessage(message) {
  const fields = [...message.replaceAll('\r\n', '\n').matchAll(/^FileSpacer-Distribution:[ \t]*(.*)$/gm)];
  if (!fields.length) return [];
  if (fields.length !== 1) throw new Error('Duplicate FileSpacer-Distribution fields.');
  const value = fields[0][1].trim();
  if (value === 'none') return [];
  const selected = value.split(',').map(channel => channel.trim());
  if (selected.some(channel => !channels.includes(channel))) {
    throw new Error('Expected winget, chocolatey or exclusive none in FileSpacer-Distribution.');
  }
  return [...new Set(selected)];
}

export function publicationForTag(message, tag, repository) {
  releaseVersion(tag);
  return {tag, repository, distribution: distributionFromMessage(message)};
}

export function selectDistribution(publication, channel, repository, manualTag = null) {
  if (!channels.includes(channel)) throw new Error('Unknown distribution channel.');
  // A manual dispatch is an explicit request, including for older GitHub-only releases.
  if (manualTag !== null) {
    releaseVersion(manualTag);
    return {tag: manualTag, enabled: true};
  }
  if (publication?.repository !== repository) throw new Error('Invalid release publication repository.');
  releaseVersion(publication.tag);
  const selected = publication.distribution ?? [];
  if (!Array.isArray(selected) || selected.some(item => !channels.includes(item))) {
    throw new Error('Invalid release publication distribution list.');
  }
  return {tag: publication.tag, enabled: selected.includes(channel)};
}

export function coordinate(event, publication, repository, inputs = {}) {
  if (event === 'schedule') return {tag: '', winget: true, chocolatey: true, dry_run: false, retry: false};
  if (event === 'workflow_dispatch') {
    if (!['all', ...channels].includes(inputs.channel)) throw new Error('Select all, winget or chocolatey.');
    const tag = inputs.tag || '';
    if (tag) releaseVersion(tag);
    const dry_run = inputs.dry_run === true, retry = inputs.retry === true;
    if (dry_run && (!tag || inputs.channel !== 'chocolatey' || retry)) {
      throw new Error('Dry run requires a tag, the chocolatey channel and no retry.');
    }
    if (retry && (!tag || inputs.channel === 'all')) throw new Error('Retry requires a tag and one channel.');
    return {tag, winget: inputs.channel !== 'chocolatey', chocolatey: inputs.channel !== 'winget', dry_run, retry};
  }
  if (event !== 'workflow_run') throw new Error('Unsupported distribution event.');
  const selected = Object.fromEntries(channels.map(channel => [channel, selectDistribution(publication, channel, repository).enabled]));
  return {tag: publication.tag, ...selected, dry_run: false, retry: false};
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const [mode, input, output] = process.argv.slice(2);
  if (mode === 'record') {
    const publication = publicationForTag(await fs.readFile(input, 'utf8'), process.env.RELEASE_TAG, process.env.GITHUB_REPOSITORY);
    await fs.writeFile(output, JSON.stringify(publication, null, 2) + '\n');
    console.log(`Distribution: ${publication.distribution.join(', ') || 'none'}`);
  } else if (mode === 'select') {
    const manual = process.env.GITHUB_EVENT_NAME === 'workflow_dispatch';
    const publication = manual ? null : JSON.parse(await fs.readFile(input, 'utf8'));
    const selection = selectDistribution(publication, process.env.DISTRIBUTION_CHANNEL, process.env.GITHUB_REPOSITORY, manual ? process.env.RELEASE_TAG : null);
    if (process.env.GITHUB_OUTPUT) await fs.appendFile(process.env.GITHUB_OUTPUT, `tag=${selection.tag}\nenabled=${selection.enabled}\n`);
    console.log(`${selection.enabled ? 'Selected' : 'Skipped'} ${process.env.DISTRIBUTION_CHANNEL} for ${selection.tag}.`);
  } else if (mode === 'coordinate') {
    const event = JSON.parse(await fs.readFile(process.env.GITHUB_EVENT_PATH, 'utf8'));
    const automatic = process.env.GITHUB_EVENT_NAME === 'workflow_run';
    if (process.env.GITHUB_EVENT_NAME === 'workflow_dispatch'
        && process.env.GITHUB_REF !== `refs/heads/${event.repository.default_branch}`) {
      throw new Error('Run distribution from the default branch.');
    }
    if (automatic && (event.workflow_run.conclusion !== 'success'
        || event.workflow_run.repository.full_name !== process.env.GITHUB_REPOSITORY)) {
      throw new Error('Expected a successful publication from this repository.');
    }
    const selection = coordinate(process.env.GITHUB_EVENT_NAME,
      automatic ? JSON.parse(await fs.readFile(input, 'utf8')) : null, process.env.GITHUB_REPOSITORY, {
        ...event.inputs, dry_run: event.inputs?.dry_run === 'true', retry: event.inputs?.retry === 'true'
      });
    await fs.appendFile(process.env.GITHUB_OUTPUT, Object.entries(selection).map(([key, value]) => `${key}=${value}\n`).join(''));
    console.log(JSON.stringify(selection));
  } else {
    throw new Error('Usage: release-distribution.mjs record <tag-message> <publication> | select|coordinate <publication>');
  }
}
