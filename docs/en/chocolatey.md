<a id="chocolatey-distribution"></a>

# Chocolatey distribution

The **Submit FileSpacer to Chocolatey** workflow runs independently when
**Publish FileSpacer** finishes successfully and Chocolatey was explicitly selected
with `release.sh X.Y.Z --distribute chocolatey`. Repeat `--distribute` to select
several channels; no option means no automatic catalog distribution.
It can also be started manually
with a published stable `vX.Y.Z` tag, including releases made before this
workflow was introduced. A manual run explicitly selects Chocolatey regardless of
the tag's original preference. A failed or cancelled release does not submit a package.

<a id="setup"></a>

## Setup

Create and confirm a free [Chocolatey community account](https://community.chocolatey.org/account/Register),
then copy its API key from the account page. Add it to the FileSpacer repository
under **Settings > Secrets and variables > Actions > New repository secret**,
with the name **CHOCO_API_KEY**. Do not commit or log the key.

The workflow and templates must be pushed to the default branch before automatic
distribution can run. GitHub loads the `workflow_run` listener and distribution
code from that branch. The existing release workflow already supplies the
`release-publication` artifact containing the exact published tag and channel list.
Automatic runs skip submission if Chocolatey is not selected or the list is absent.

<a id="publication"></a>

## Publication

The workflow calls `.github/scripts/chocolatey.mjs` to download the official
versioned installer and `FileSpacer-update.txt`. It reuses the WinGet script's
stable-version and installer integrity checks, comparing the downloaded size and
SHA-256 with the update manifest and GitHub asset digest when available.

The generator fills the templates in `installer/chocolatey`. The resulting
`filespacer` package contains metadata and PowerShell scripts, without embedding
the application binary. The client downloads the official installer and verifies
the pinned SHA-256. Installation uses `/S /AllUsers`, requires Windows x64 and
administrative rights, and preserves the native installer's upgrade choices.
It does not request a change of default file manager. Close FileSpacer before
installing or upgrading; a conflicting current-user installation must be resolved
with the normal installer first.

Before submission, GitHub runs unit tests, builds the `.nupkg` using `choco pack`,
and tests actual installation and uninstallation on the disposable Windows runner.
The uninstall script targets only the machine-wide FileSpacer registration and
calls its native uninstaller with `/S /AllUsers _?=<installation directory>`.
The documented NSIS `_?=` option makes that call wait for the actual uninstall;
after it exits, the package removes the remaining uninstaller and the directory
only if it is empty. A personal installation is not
selected by that script. The key is exposed only to the final `choco push` step.
Package sources and the `.nupkg` are retained as a workflow artifact for review.

Submission goes to `https://push.chocolatey.org/` and is subject to Chocolatey's
validation and moderation. A successful upload does not mean immediate public
availability. Check the [package page](https://community.chocolatey.org/packages/filespacer)
and respond to any maintainer feedback. No new version tag or GitHub release is
created by distribution, and its failure does not change the successful release.

<a id="retry-and-use"></a>

## Retry and use

For a distribution failure, open **Actions > Submit FileSpacer to Chocolatey >
Run workflow**, choose the default branch, and enter the already published tag.
The equivalent client command is
`gh workflow run chocolatey.yml --ref main -f tag=vX.Y.Z`.
Enable **dry_run** to validate packaging, installation and uninstallation without
submitting anything or using the API key.
This rebuilds the package from the same public release assets. It does not rebuild
the application. An existing version may be rejected by the server: inspect its
submission/moderation status before retrying. Do not force an overwrite of an
approved version or replace its published installer. Packaging corrections for a
version already published require following Chocolatey's package revision rules.

Once the first version is accepted, users can run these commands from an elevated
terminal:

```powershell
choco install filespacer --yes
choco upgrade filespacer --yes
choco uninstall filespacer --yes
```

<a id="local-checks"></a>

## Local checks

Run in CMD from the repository root:

```cmd
node --test .github/scripts/chocolatey.test.mjs .github/scripts/winget.test.mjs
```

The unit tests simulate HTTP and Chocolatey helper calls; they do not install
anything or access the user registry. `choco pack` validates package creation.
The runner smoke test exercises the real installer and uninstaller; it does not
validate an application upgrade or replace the application's regression suite.
