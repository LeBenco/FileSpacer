# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
import json
from pathlib import Path
import runpy
import tempfile
import unittest

script = runpy.run_path(str(Path(__file__).with_name("update-release-links.py")))
replace_link, update, fingerprint = (script[key] for key in ("replace_link", "update", "fingerprint"))
project = script["PROJECT"]
url = f"{project}/releases/download/v1.3.2/FileSpacer-1.3.2-install.exe"
release = {"tag_name": "v1.3.2", "draft": False, "prerelease": False,
           "assets": [{"name": "FileSpacer-1.3.2-install.exe", "browser_download_url": url}]}


class ReleaseLinkTests(unittest.TestCase):
    def test_preserves_markdown_translation_attributes_quotes_and_line_endings(self):
        for opening in ('<a data-href="keep" href="old" id="last-release-url">',
                        "<A class='download' ID='last-release-url'\r\n HREF = 'old'>"):
            with self.subTest(opening=opening):
                content = '# Titre éλληνικά\r\n\r\n- Télécharger ' + opening + 'la dernière version</a>.\r\n'
                result = replace_link(content, url, (1, 3, 2))
                self.assertEqual(result, content.replace('old', url))
                self.assertEqual(replace_link(result, url, (1, 3, 2)), result)

    def test_ignores_comments_and_rejects_missing_duplicate_or_invalid_targets(self):
        anchor = '<a id="last-release-url" href="old">latest</a>'
        self.assertEqual(replace_link('<!-- ' + anchor + ' -->\n' + anchor, url, (1, 3, 2)),
                         '<!-- ' + anchor + ' -->\n' + anchor.replace('old', url))
        for content in ('missing', anchor + anchor, '<a id="last-release-url">latest</a>',
                        '<span id="last-release-url" href="old">latest</span>'):
            with self.subTest(content=content), self.assertRaises(ValueError):
                replace_link(content, url, (1, 3, 2))
        with self.assertRaises(ValueError):
            replace_link(anchor.replace('old', url.replace('1.3.2', '1.4.0')), url, (1, 3, 2))

    def test_updates_all_locales_without_blessing_stale_translations_or_partial_errors(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            english = '# FileSpacer\nDownload <a id="last-release-url" href="old">latest</a>.\n'
            (root / 'README.md').write_text(english, encoding='utf-8')
            for locale, source_hash in [('fr', fingerprint(english)), ('de', 'stale')]:
                folder = root / 'docs' / locale
                folder.mkdir(parents=True)
                (folder / 'locale.json').write_text('{}', encoding='utf-8')
                (folder / 'README.md').write_text(english.replace('Download', locale), encoding='utf-8')
                (folder / 'source-state.json').write_text(json.dumps({'sources': {
                    'README.md': {'sourceHash': source_hash, 'headings': [['filespacer', 2]]},
                    'docs/user-guide.md': {'sourceHash': 'unrelated'}}}), encoding='utf-8')
            bad = root / 'docs/de/README.md'
            bad.write_text('missing', encoding='utf-8')
            with self.assertRaises(ValueError):
                update(root, release)
            self.assertEqual((root / 'README.md').read_text(encoding='utf-8'), english)
            bad.write_text(english, encoding='utf-8')
            self.assertEqual(len(update(root, release)), 4)
            self.assertEqual(update(root, release), [])
            for locale in ('fr', 'de'):
                state = json.loads((root / f'docs/{locale}/source-state.json').read_text(encoding='utf-8'))
                expected = fingerprint(english.replace('old', url)) if locale == 'fr' else 'stale'
                self.assertEqual(state['sources']['README.md']['sourceHash'], expected)
                self.assertEqual(state['sources']['docs/user-guide.md']['sourceHash'], 'unrelated')
            for invalid in ({**release, 'draft': True}, {**release, 'prerelease': True},
                            {**release, 'tag_name': 'v1.3.2-rc1'}, {**release, 'assets': []}):
                with self.subTest(release=invalid), self.assertRaises(ValueError):
                    update(root, invalid)


if __name__ == '__main__':
    unittest.main()
