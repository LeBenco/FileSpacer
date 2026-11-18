<a id="website-generation"></a>

# Website generation

The public site generates a separate page for `README.md`, the user guide, the
changelog and third-party notices. A horizontal menu links these documents and
GitHub. Each page's sidebar shows its subsections directly, below the language
selector. The white header contains the application icon and name on the left
and search on the right. The same CSS lets the horizontal menu use the empty
space to its left when necessary, preserving sidebar alignment when it fits.
Privacy and local-data maintenance have separate sections
in the user guide. The short standalone privacy policy remains on GitHub; its
former site anchors still reach the relevant guide sections.
The original English documents remain
the source of the English site and the repository documentation. Complete
translations live under `docs/<language>/`, with documents directly in that
directory and their own images in its `images/` subdirectory. English technical
and user documents are in `docs/en/`; the English README, changelog and third-party
notices stay at the repository root as the English website sources.
Build instructions, tests and technical notes remain on GitHub: links to files
not included in the site become links to the repository's `main` branch.
Online changelog dates come from `published_at` in the GitHub Releases JSON API,
matched by release version. The existing Markdown files supply the translated
notes. `Intl.DateTimeFormat` uses each site's locale for numeric day, month and
four-digit year formatting, keeping the UTC publication day. Source dates are
not parsed or used as a fallback. The public changelog includes only versions
present in GitHub's published stable releases, with their notes and navigation
entries filtered together in every language. Never add an `Unreleased` section
or planned features: changelog sections must name numbered released versions.
The generator rejects non-versioned sections, including in offline builds.
Existing heading IDs remain unchanged, including their original
date suffixes, so published bookmarks still work.
The generator fetches all release pages during a web build, and incorporates
the publication metadata into the build fingerprint. API failures stop the build
before replacing the existing output. Reading the finished site needs no API
request. CI uses its automatic GitHub token; local builds can use `GH_TOKEN` or
`GITHUB_TOKEN`, or access the public API without authentication. Offline help
keeps its local numbered changelog, so an installer can include its own release
notes before the corresponding GitHub release exists.
The Nilesoft configuration reference has a translated file for each language;
its GitHub links follow the selected language. Polish, Dutch and Ukrainian also
provide translated developer guides; their references use the selected language.

The generator discovers language directories containing `locale.json` and all
four translated source files, the privacy policy and the localized Nilesoft
reference. It creates the
language selector above the sidebar navigation from those directories, using
each language's own name and a local SVG flag. The country comes from the
locale's region, with no separate list to maintain. Each full generation queries
Wikidata for the current preferred flag image of each country (ISO code `P297`,
flag image `P41`), excluding dissolved countries. Missing or ambiguous results
stop generation instead of selecting an arbitrary variant.
The Commons API supplies the original SVG URL and license metadata. Only
public-domain or CC0 files explicitly requiring no attribution are accepted.
The SVGs are downloaded once per country, optimized with SVGO using its default
preset and multiple passes, and included under `assets/flags/` in every language build,
including offline help. No country-specific file names or corrections are kept. Proportions are preserved.
Local SVG illustrations, including the header icon, are optimized when copied
into the generated site. Editor metadata is removed; source artwork stays editable
and its copyright/license comments are retained in the generated illustration.
Flags in the closed language menu use lazy loading; the current flag has a fixed
layout box and loads normally. Visitors make no requests to Wikimedia.
Failed metadata requests, incompatible licenses or invalid SVG responses stop
generation before the previous finished output is replaced. Temporary sources
are cleaned by the shared build wrapper. A full rebuild needs network access
to Wikidata and Wikimedia Commons; reading the generated site does not.

`assets/favicon.ico` contains the application's original 16 × 16 icon frame,
extracted from `src/res/FileSpacer.ico` without resampling. Keep it in sync
when regenerating the application icon. The theme's `favicon_ico` setting
uses this asset in every language.

Heading order and hierarchy must match English. The generator normalizes levels,
uses explicit English heading anchors shared by every source language and rewrites
internal links. Translated titles do not determine the canonical IDs. Changing
language keeps the current document and appends its fragment to the other language’s URL, without a
translation map. Localized assets have identical filenames and relative URLs
within each generated language directory. Missing
documents, headings or localized PNG/GIF captures fail the build. The neutral
application logo is shared. See [translation maintenance](translations/README.md)
for terminology sources and naming conventions.

Each language gets its own Just the Docs build, navigation, search index,
interface labels and media, using the same pinned theme and framing. The small
upstream layout override translates its skip link and versions the compiled
theme script so browsers receive search changes after deployment. Retain its
MIT notice and review it when updating the theme. The repository README has no
language selector or language links.
Lunr and the theme script use `defer` in their original dependency order, so their
downloads do not block HTML parsing. Search, the mobile menu and offline search
keep the same theme implementation.
The Website section of third-party notices credits Just the Docs, Jekyll, Lunr,
Wikimedia Commons/Wikidata and SVGO in every language. The sidebar and mobile footer no longer
repeat the theme credit. Keep the empty `nav_footer_custom.html` override to
suppress the theme's default footer. The theme's full MIT notice remains in the
source layout directory and the generated `JustTheDocs-LICENSE.txt`; Lunr's
notice is bundled under `assets/licenses/`.

Download links and badges come from
the Markdown sources; the generator does not add a second download button or
derive one from the changelog.

PNG and GIF screenshots are small linked thumbnails. A local script opens them
in a native HTML dialog with a localized close button, Escape, backdrop dismissal
and focus restoration. Links still open the image when JavaScript or native
dialogs are unavailable. The theme has no built-in lightbox; no additional
library or CDN is used. SVG logos are not treated as screenshots.
Screenshot URLs include a content hash, so replacing an image under the same
filename refreshes cached thumbnails and popup images automatically.
Screenshots use lazy loading so content further down does not load immediately.
Animated WebP variants are used in both website and offline builds when
`website/animations.json` matches the source GIF's SHA-256. GIFs remain the
Markdown sources for GitHub. A changed GIF or missing variant falls back to
the current GIF, so a new capture cannot display an outdated animation.
Regenerate the lossless variants with the
[capture optimization script](../../../tools/documentation-capture/README.md#website-animation-compression).

Every page has its own title, description, canonical URL and reciprocal
`hreflang` links to the equivalent documents, including an English `x-default`.
The four page descriptions are maintained in each language's `locale.json`;
they are not extracted from badges or individual changelog entries.
The root `sitemap.xml` lists all generated pages and their language variants.
Both use the discovered language directories. Existing single-page bookmarks
are redirected to the matching document while retaining their fragment.
Temporary Jekyll source directories are removed after a build, including failures.

The theme's `lunr/custom-index.js` hook uses Unicode word trimming for translated
sites and omits English stemming and stop words. Lunr's default ASCII trimmer
would otherwise discard Greek words. English keeps the original search pipeline.
The test dependency matches the theme's Lunr 2.3.9 runtime.

<a id="local-checks"></a>

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
site root, other languages under `/de/`, `/el/`, `/es/`, `/fr/`, `/it/`, `/nl/`,
`/pl/`, `/pt/` and `/uk/`.
These outputs are generated and ignored by Git. The application build and its
dependencies are separate from this documentation build.

The [documentation capture scripts](../../../tools/documentation-capture/README.md)
keep the menu, movement and resizing animations reproducible with ScreenToGif,
localized fictional files and the same local wallpaper. Review the exported
media before replacing the files under `docs/<language>/images/`.

<a id="shared-website-and-offline-builds"></a>

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

<a id="github-pages"></a>

## GitHub Pages

The Pages workflow builds and checks relevant pull requests. It generates the
sources once, reads `locales.json`, builds each language and deploys the result.
No second language list needs updating in CI. Pushes to `main` rebuild and deploy
when any README, `BUILD.md`, a file under `docs/`, a translation, icon, generator,
shared documentation action or Pages workflow changes. Pull requests use the same
path filters and only build the site. It can also be started manually.

After a successful release, the Pages workflow also runs through `workflow_run`.
Before building the default branch on a push, manual run or successful release,
it updates the `href` of the reserved `last-release-url` anchor in every README
from the latest published installer. The Python HTML parser preserves translated
text and formatting; synchronized README source references advance with the URL.
It commits actual changes using the automatic token, then builds the updated
branch in the same run. Pull requests only build and never write to the branch.
There is no separate release-link workflow or recursive workflow dispatch.

Pages and release share the `build-docs`
action with `web` and `offline` targets respectively. Version-tag pushes
start the release workflow, whose offline documentation waits for application
tests and uses their exact commit. A documentation push does not start an
application release; changing both documentation and application code in one
commit still produces a single Pages run for that branch push.

English document updates do not wait for translation updates. The shared
generator retains each translation and its recorded section IDs, and reports
outdated documents as workflow annotations and a summary table. See the
[translation guide](translations/README.md) for recording synchronized sources.

Once the workflow has been pushed, select **Settings > Pages > Source > GitHub
Actions**. The expected URL is `https://lebenco.github.io/FileSpacer/`; `_config.yml`
sets that project path so assets and search work under `/FileSpacer/`.

The repository's **About > Website** field can show the published address.
Changing the included documents requires updating `sources` in `generate.mjs`.
The Pages path filters already cover all of `docs/` and the README files; add a
filter if a new generator input lives outside the existing paths.
