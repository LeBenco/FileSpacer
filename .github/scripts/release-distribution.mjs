// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import fs from 'node:fs/promises';
import {pathToFileURL} from 'node:url';

const channels = ['winget', 'chocolatey'];
function checkTag(tag) {
  if (typeof tag !== 'string' || tag !== tag.trim() || !/^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.test(tag)) {
    throw new Error('Expected a stable vX.Y.Z release tag.');
  }
}

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
  checkTag(tag);
  return {tag, repository, distribution: distributionFromMessage(message)};
}

export function selectDistribution(publication, channel, repository, manualTag = null) {
  if (!channels.includes(channel)) throw new Error('Unknown distribution channel.');
  // A manual dispatch is an explicit request, including for older GitHub-only releases.
  if (manualTag !== null) {
    checkTag(manualTag);
    return {tag: manualTag, enabled: true};
  }
  if (publication?.repository !== repository) throw new Error('Invalid release publication repository.');
  checkTag(publication.tag);
  const selected = publication.distribution ?? [];
  if (!Array.isArray(selected) || selected.some(item => !channels.includes(item))) {
    throw new Error('Invalid release publication distribution list.');
  }
  return {tag: publication.tag, enabled: selected.includes(channel)};
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
  } else {
    throw new Error('Usage: release-distribution.mjs record <tag-message> <publication> | select <publication>');
  }
}
