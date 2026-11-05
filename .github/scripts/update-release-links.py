# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
"""Update only the href of the reserved last-release-url anchor in each README."""
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import sys

PROJECT = "https://github.com/LeBenco/FileSpacer"


class ReleaseLinkParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links = []

    def handle_starttag(self, tag, attrs):
        if ("id", "last-release-url") not in attrs:
            return
        if tag != "a" or sum(key == "href" for key, _ in attrs) != 1:
            raise ValueError("last-release-url must identify an anchor with one href")
        self.links.append((self.getpos(), self.get_starttag_text()))


def replace_link(content, url, version):
    parser = ReleaseLinkParser()
    parser.feed(content)
    parser.close()
    if len(parser.links) != 1:
        raise ValueError("Expected exactly one last-release-url anchor")
    (line, column), opening = parser.links[0]
    hrefs = list(re.finditer(r'''\shref\s*=\s*(["'])(.*?)\1''', opening, re.I | re.S))
    if len(hrefs) != 1:
        raise ValueError("Expected one quoted href in the installer anchor")
    href = hrefs[0]
    previous = re.fullmatch(re.escape(PROJECT) +
                            r"/releases/download/v(\d+)\.(\d+)\.(\d+)/FileSpacer-\1\.\2\.\3-install\.exe",
                            href.group(2))
    if previous and tuple(map(int, previous.groups())) > version:
        raise ValueError("Cannot replace a newer installer link with an older release")
    offset = sum(map(len, content.splitlines(keepends=True)[:line - 1])) + column
    return content[:offset + href.start(2)] + url + content[offset + href.end(2):]


def fingerprint(content):
    return hashlib.sha256(content.replace("\r\n", "\n").encode("utf-8")).hexdigest()


def update(root, release):
    tag = release["tag_name"]
    if release["draft"] or release["prerelease"] or not re.fullmatch(r"v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)", tag):
        raise ValueError("Expected a published stable vX.Y.Z release")
    version = tuple(map(int, tag[1:].split(".")))
    name = f"FileSpacer-{tag[1:]}-install.exe"
    url = f"{PROJECT}/releases/download/{tag}/{name}"
    assets = [asset for asset in release["assets"] if asset["name"] == name]
    if len(assets) != 1 or assets[0]["browser_download_url"] != url:
        raise ValueError("The official versioned installer is missing from the release")
    locales = sorted((root / "docs").glob("*/locale.json"))
    readmes = [root / "README.md", *(locale.parent / "README.md" for locale in locales)]
    contents = {file: file.read_bytes().decode("utf-8") for file in readmes}
    replacements = {file: replace_link(content, url, version) for file, content in contents.items()}
    old_hash = fingerprint(contents[readmes[0]])
    new_hash = fingerprint(replacements[readmes[0]])
    for locale in locales:
        file = locale.parent / "source-state.json"
        original = file.read_bytes().decode("utf-8")
        state = json.loads(original)
        reference = state["sources"]["README.md"]
        # Do not mark unrelated, untranslated English changes as synchronized.
        if reference["sourceHash"] == old_hash and old_hash != new_hash:
            reference["sourceHash"] = new_hash
            newline = "\r\n" if "\r\n" in original else "\n"
            replacements[file] = (json.dumps(state, ensure_ascii=False, indent=2) + "\n").replace("\n", newline)
    changed = []
    # Validate all documents before writing any of them.
    for file, content in replacements.items():
        encoded = content.encode("utf-8")
        if file.read_bytes() != encoded:
            file.write_bytes(encoded)
            changed.append(file.relative_to(root).as_posix())
    return changed


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[2]
    release = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    print("\n".join(update(root, release)) or "Installer links are already current.")
