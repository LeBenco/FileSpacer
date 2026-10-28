# Website generation

The public site concatenates `README.md`, the user guide, the privacy policy,
the changelog and third-party notices. The original English documents remain
the source of the English site and the repository documentation. Complete
website-only translations live under `website/translations/<language-region>/`.
Build instructions, tests and technical notes remain on GitHub: links to files
not included in the site become links to the repository's `main` branch.

The generator discovers language directories containing `locale.json` and all
five translated source files. It creates the language selector above the sidebar
navigation from those directories, using each language's own name and a Unicode
flag. The bundled, OFL-licensed Noto Color Emoji flag font supplies flags on
Windows as well; it requires no external font service.

Heading order and hierarchy must match English. The generator normalizes levels,
reuses the English document-specific heading IDs in every language and rewrites
internal links. Changing language preserves the section being read. Missing
documents, headings or localized PNG/GIF captures fail the build. The neutral
application logo is shared. See [translation maintenance](translations/README.md)
for terminology sources and naming conventions.

Each language gets its own Just the Docs build, navigation, search index,
interface labels and media, using the same pinned theme and framing. The small
upstream layout override translates its skip link and versions the compiled
theme script so browsers receive search changes after deployment. Retain its
MIT notice and review it when updating the theme. The repository README has no
language selector or language links.

The theme's `lunr/custom-index.js` hook uses Unicode word trimming for translated
sites and omits English stemming and stop words. Lunr's default ASCII trimmer
would otherwise discard Greek words. English keeps the original search pipeline.
The test dependency matches the theme's Lunr 2.3.9 runtime.

## Local checks

With Node.js 22 or later, run from the repository root:

```cmd
npm ci --prefix website --ignore-scripts
npm test --prefix website
npm run generate --prefix website
```

The result under `build/site-source` is Jekyll input, not the finished website.
English is at its root; the other inputs are under `_locales/<language-region>`.
`locales.json` describes the discovered builds and their output paths. Build each
source with Jekyll and merge its result at the manifest's `path`: English at the
site root, other languages under `/de/`, `/el/`, `/es/`, `/fr/`, `/it/` and `/pt/`.
These outputs are generated and ignored by Git. The application build and its
dependencies are separate from this documentation build.

## Shared website and offline builds

`.github/scripts/build-docs.mjs` owns the complete generation pipeline: it calls
this generator, builds every discovered language with Jekyll and retains the same
pinned theme, localized navigation, search indexing and media. Its build-only Ruby
dependencies are locked in `.github/scripts/Gemfile.lock`. Node.js 22+, Ruby 3.3
and Bundler are needed to generate finished pages; none is needed to read them.

From CMD at the repository root, install the dependencies once:

```cmd
npm ci --prefix website --ignore-scripts
cd .github\scripts
bundle install
cd ..\..
node .github\scripts\build-docs.mjs --target web
node .github\scripts\build-docs.mjs --target offline
```

The outputs are `build/site` and `build/offline-docs`. The offline variant resolves
site links and resources relative to each HTML page and embeds its search data
in the existing theme script, so search works from `file://` without a server.
The theme's indexing and localized tokenization remain unchanged. A changed theme
loader fails generation until its offline adaptation has been reviewed.

Both workflows invoke `.github/actions/build-docs`, which installs the same
dependencies and calls this script with the appropriate target. The release
workflow checks out the version tag for documentation and application alike;
it never downloads the published website or waits for the Pages workflow.

The offline output includes a source fingerprint, file hashes and discovered
languages. Installer packaging verifies these before reusing a generated copy.
Text input hashes normalize CRLF/LF to permit Linux-to-Windows artifact transfer.
An absent, stale or damaged copy is rebuilt with Jekyll. A generated NSIS include
installs the documentation and removes only its owned files on uninstallation;
unowned files in the help directory are preserved.

## GitHub Pages

The Pages workflow builds and checks relevant pull requests. It generates the
sources once, reads `locales.json`, builds each language and deploys the result.
No second language list
needs updating in CI. Pushes to `main` rebuild and deploy when a source document,
translation, capture, icon, generator or workflow changes. It can also be started
manually. The workflow does not create application releases, tags or installers.

Once the workflow has been pushed, select **Settings > Pages > Source > GitHub
Actions**. The expected URL is `https://lebenco.github.io/FileSpacer/`; `_config.yml`
sets that project path so assets and search work under `/FileSpacer/`.

The repository's **About > Website** field can show the published address.
Changing the included documents requires updating `sources` in `generate.mjs`
and the workflow's path filters together.
