// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import fs from 'node:fs/promises';
import {spawnSync} from 'node:child_process';
import {parseArgs} from 'node:util';

const {values} = parseArgs({options: {pandoc: {type: 'string'}}});
const pandoc = values.pandoc || process.env.PANDOC || 'pandoc';

const unreleased = new Set(['Unreleased', 'Non publié', 'Unveröffentlicht', 'Non pubblicato', 'Sin publicar', 'Não publicado', 'Μη δημοσιευμένο', 'Niewydane', 'Nog niet uitgebracht', 'Ще не випущено']);

for (const [language, locale] of [['en', null], ['fr', 'fr-FR'], ['de', 'de-DE'], ['it', 'it-IT'], ['es', 'es-ES'], ['pt', 'pt-PT'], ['el', 'el-GR'], ['pl', 'pl-PL'], ['nl', 'nl-NL'], ['uk', 'uk-UA']]) {
  const source = locale ? `docs/${language}/CHANGELOG.md` : 'CHANGELOG.md';
  const lines = (await fs.readFile(source, 'utf8')).replace(/^\uFEFF/, '').split(/\r?\n/);
  const notes = lines.filter(line => !line.startsWith('# ') && !line.startsWith('[')
      && !/^\s*<a\b[^>]*>\s*<\/a>\s*$/i.test(line)
      && !unreleased.has(line.replace(/^#+\s+/, '')))
    .join('\n').trim() + '\n';
  const converted = spawnSync(pandoc, [
    '--from=gfm', '--to=rtf', '--standalone',
    '--output', `build/installer-assets/changelog-${language}.rtf`,
  ], {input: notes, encoding: 'utf8', timeout: 30000});
  if (converted.error) throw new Error('Pandoc is required for installer notes. Put it on PATH or provide --pandoc.', {cause: converted.error});
  if (converted.status !== 0) throw new Error(`Pandoc failed for ${language}: ${converted.stderr}`);
}
