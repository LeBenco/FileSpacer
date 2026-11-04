// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import fs from 'node:fs/promises';

const unreleased = new Set(['Unreleased', 'Non publié', 'Niewydane', 'Nog niet uitgebracht', 'Ще не випущено']);

for (const [language, locale] of [['en', null], ['fr', 'fr-FR'], ['pl', 'pl-PL'], ['nl', 'nl-NL'], ['uk', 'uk-UA']]) {
  const source = locale ? `website/translations/${locale}/CHANGELOG.md` : 'CHANGELOG.md';
  const lines = (await fs.readFile(source, 'utf8')).replace(/^\uFEFF/, '').split(/\r?\n/);
  const notes = lines.filter(line => !line.startsWith('# ') && !line.startsWith('[')
      && !unreleased.has(line.replace(/^#+\s+/, '')))
    .map(line => line.replace(/^#+\s+/, '')).join('\r\n').trim() + '\r\n';
  await fs.writeFile(`build/installer-assets/changelog-${language}.txt`, Buffer.from('\uFEFF' + notes, 'utf16le'));
}
