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
upstream layout override only translates its skip link; retain its MIT notice
and review it when updating the theme. The repository README has no language
selector or language links.

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

## GitHub Pages

The Pages workflow builds and checks relevant pull requests. It generates the
sources once, reads `locales.json` to create its build matrix, builds each language
independently, then merges and deploys their artifacts. No second language list
needs updating in CI. Pushes to `main` rebuild and deploy when a source document,
translation, capture, icon, generator or workflow changes. It can also be started
manually. The workflow does not create application releases, tags or installers.

Once the workflow has been pushed, select **Settings > Pages > Source > GitHub
Actions**. The expected URL is `https://lebenco.github.io/FileSpacer/`; `_config.yml`
sets that project path so assets and search work under `/FileSpacer/`.

The repository's **About > Website** field can show the published address.
Changing the included documents requires updating `sources` in `generate.mjs`
and the workflow's path filters together.
