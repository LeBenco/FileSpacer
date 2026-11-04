# WinGet distribution

GitHub Actions submits `Benco.FileSpacer` to the public
[`microsoft/winget-pkgs`](https://github.com/microsoft/winget-pkgs) repository
when **Publish FileSpacer** completes successfully (`workflow_run`). Distribution
has its own run and result; failure does not change the successful release run.
The workflow can also be dispatched for an existing published stable release tag.

## One-time setup

1. Create a GitHub **personal access token (classic)** with **public_repo**,
   using the account that will own the WinGet submissions and its fork of
   `microsoft/winget-pkgs`. WingetCreate does not support fine-grained PATs.
   See [Microsoft's token instructions](https://github.com/microsoft/winget-create/blob/main/doc/token.md).
2. In the FileSpacer repository, open **Settings > Secrets and variables >
   Actions**, and add a repository secret named **WINGET_TOKEN** containing
   that token. Do not put the token in a workflow, commit, or log. Renew this
   secret when the token expires or is revoked.
3. Commit and push the distribution tooling to the default branch before
   creating the next release tag. GitHub loads a `workflow_run` listener from the
   default branch. It works after a release published using `GITHUB_TOKEN`, which
   would not itself trigger another workflow's `release` event.

The release's ordinary `GITHUB_TOKEN` remains sufficient for publishing
FileSpacer itself. The additional PAT is used only by WingetCreate to submit
to the external community repository. No manual first-package creation is
needed: generated manifests support both the first submission and new versions.

## Each release

Follow the existing [release procedure](build.md#releases). Once publication
succeeds, the independent WinGet workflow downloads `release-publication` from
that exact successful run. This small artifact records the published tag, also
for releases started manually with a tag input. It then:

1. Checks that the tag is a stable `vX.Y.Z` release, and skips versions already
   present in `winget-pkgs` or with an open submission PR.
2. Downloads the versioned public installer and `FileSpacer-update.txt`.
   Verifies the installer size and SHA-256 against the update manifest and,
   when available, GitHub's asset digest. It does not build another installer.
3. Generates and retains the version, English metadata, and installer YAML
   manifests as a workflow artifact. Installer entries cover Windows x64,
   current-user `/CurrentUser` and machine `/AllUsers` installations, silent
   `/S` execution, and NSIS's final `/D=<INSTALLPATH>` directory argument.
4. Installs .NET 9 and downloads WingetCreate 1.12.13.0, checks its pinned SHA-256
   and Microsoft Authenticode signature, then validates and submits manifests using
   `wingetcreate submit`. The PAT is supplied through the documented
   `WINGET_CREATE_GITHUB_TOKEN` environment variable.

The installer retains its existing component defaults and saved upgrade choices.
The WinGet manifest does not request a change of default file manager.
WinGet submission does not mean immediate catalog availability: Microsoft's
automated checks and review must accept the PR. Monitor the PR link in the
job log and respond to review requests if necessary.

If submission fails, the GitHub release and its successful workflow result are
preserved; only the distribution workflow reports failure. Fix the cause, then
open **Actions > Submit FileSpacer to WinGet > Run workflow**, select the default
branch, and enter the published
tag. This retries only distribution, without rebuilding or republishing the
application. A missing secret also fails explicitly, after retaining manifests.
This manual entry point also supports releases made before the metadata artifact
was introduced, or published outside the normal release workflow. Failed or
cancelled release workflows do not submit anything.
Existing merged versions are skipped; this workflow does not replace their
installers or manifests. Keep published versioned release assets unchanged.

After the first PR is accepted and the catalog is refreshed:

```powershell
winget install --id Benco.FileSpacer --exact --scope user
winget install --id Benco.FileSpacer --exact --scope machine
winget upgrade --id Benco.FileSpacer --exact
winget uninstall --id Benco.FileSpacer --exact
```

## Validation

Run the distribution tests in CMD:

```cmd
node --test .github/scripts/winget.test.mjs
```

These cover manifest generation, release eligibility, updater/asset integrity,
both installation scopes, and retries using simulated HTTP responses. They do
not exercise NSIS or the real Shell. The submission command validates the
manifest schema on the GitHub Windows runner; real installation, upgrade,
uninstallation, and acceptance are also checked during the upstream review.
Before the first submission, verify both installation scopes in an isolated
Windows VM with the actual released installer. Generated manifests are
available as an artifact even if submission fails.
