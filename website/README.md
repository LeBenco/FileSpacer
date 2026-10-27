# Website generation

The public site concatenates `README.md`, the user guide, the privacy policy,
the changelog and third-party notices. Edit those source documents directly.
Build instructions, tests and technical notes remain on GitHub: links to files
not included in the site become links to the repository's `main` branch.

The generator normalizes heading levels, gives headings a document-specific ID,
rewrites internal links and copies referenced images. Missing internal headings
fail the build. The generated sidebar uses the standard Just the Docs markup;
the rest of the layout and search come from the pinned upstream theme.

## Local checks

With Node.js 22 or later, run from the repository root:

```cmd
npm ci --prefix website --ignore-scripts
npm test --prefix website
npm run generate --prefix website
```

The result under `build/site-source` is Jekyll input, not the finished website.
It is generated and ignored by Git. The application build and its dependencies
are separate from this documentation build.

## GitHub Pages

The Pages workflow builds and checks relevant pull requests. Pushes to `main`
rebuild and deploy when a source document, icon, generator or workflow changes.
It can also be started manually. The workflow does not create application
releases, tags or installers.

Once the workflow has been pushed, select **Settings > Pages > Source > GitHub
Actions**. The expected URL is `https://lebenco.github.io/FileSpacer/`; `_config.yml`
sets that project path so assets and search work under `/FileSpacer/`.

The repository's **About > Website** field can show the published address.
Changing the included documents requires updating `sources` in `generate.mjs`
and the workflow's path filters together.
