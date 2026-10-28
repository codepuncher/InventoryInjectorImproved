# Releasing

## Versioning

The version isn't stored in the repo.
The Release workflow computes it from the latest `vX.Y.Z` tag and its `bump` input (`scripts/next-version.sh`), then hands it to CMake as `I5_VERSION`.
CMake embeds it in the DLL, and the co-save validity check reads it.
Local builds use the `0.0.0` default.

## Making a release

1. Open **Actions > Release > Run workflow** on `main`.
   Leave `kind` as `release` and set `bump` to `patch`, `minor`, `major`, or an explicit `X.Y.Z`.
   The workflow builds the plugin with that version, packages it via `scripts/package.sh`, tags the commit `vX.Y.Z`, and publishes a GitHub Release with the zip and PDB.
   A release run from any branch other than `main` fails.
   Release and test runs both fail on a commit that already has a release tag.
2. `nexus-upload.yml` auto-triggers once `release.yml` finishes. Approve the pending deployment under the `nexus` environment (on the workflow run's page, or the repo's Environments tab) to let the upload proceed. If it doesn't fire, or you need to re-run a failed upload, trigger it manually via workflow_dispatch with the version (no `v` prefix); it must match the tag exactly, since the workflow resolves the tag from the checked-out commit and fails outright if that tag doesn't exist. See [Nexus Mods upload](#nexus-mods-upload) for the environment setup.
3. If the Nexus page changed, regenerate it (see below) and paste it into the Nexus Mods page editor.

## Test builds

To give someone a build of a branch, run **Actions > Release > Run workflow** on that branch with `kind` set to `test`.
Set `label` to a short `[a-z0-9-]+` name, for example `vr-support`.
The workflow publishes a GitHub prerelease tagged `vX.Y.Z-<label>.<run number>`, where `X.Y.Z` is the version `bump` gives.
The DLL inside reports `X.Y.Z.N`, with `N` from 1 to 255 taken from the run number, so a tester's cache doesn't carry over to the final release.
Test builds aren't marked as the latest release, never upload to Nexus, and don't count as the base for the next release.

## Nexus Mods page

The `<!-- nexus:start/end -->` block in [README.md](../README.md) is the **source of truth** for Features, Requirements, Installation, Compatibility, and Console commands. [nexus-page.md](nexus-page.md) holds the rest of the page (short description, overview intro, FAQ, credits).

To update the Nexus page:

1. Edit Features, Requirements, Installation, Compatibility, or Console commands inside the `<!-- nexus:start/end -->` block in [README.md](../README.md).
2. Edit the short description, overview intro, FAQ, and credits directly in [nexus-page.md](nexus-page.md).
   > **Do not edit** the `<!-- generated:start/end -->` block in [nexus-page.md](nexus-page.md): it is overwritten every time the script runs.
3. Generate the combined BBCode output:

```bash
python3 scripts/generate-nexus-page.py

# Or copy straight to the clipboard (wl-copy, xclip, xsel or pbcopy):
python3 scripts/generate-nexus-page.py --copy
```

4. Paste the output into the Nexus Mods page editor.

## Nexus Mods upload

`nexus-upload.yml` runs on a `workflow_run` trigger chained off `release.yml`: `release.yml` publishes the GitHub Release using the default `GITHUB_TOKEN`, and GitHub doesn't run a `release: published`-triggered workflow off events caused by `GITHUB_TOKEN`, so `workflow_run` is used instead (it fires on the completion of `release.yml` itself, regardless of what token published inside it). The `upload` job runs under the `nexus` [GitHub environment](https://docs.github.com/en/actions/deployment/targeting-different-environments/using-environments-for-deployment), which requires manual approval before the job proceeds and scopes the Nexus secrets to that environment. It can also be triggered manually via **workflow_dispatch** with the version (no `v` prefix), e.g. to re-run a failed upload; this still requires the same environment approval.

Because `workflow_run` always runs the copy of `nexus-upload.yml` committed to `main` (not whatever's on a feature branch), changes to this workflow only take effect after merging to `main`.

Do not restrict the `nexus` environment to tags: `workflow_run`-triggered jobs always execute against the default branch's ref (`refs/heads/main`), not the tag that triggered the upstream `release.yml` run, so a tag-only policy would silently block every auto-triggered upload. A deployment-branch policy limited to `main` is compatible with the auto-trigger and keeps `workflow_dispatch` runs from other branches from reaching the secrets.

**Prerequisites (one-time setup):**

1. Upload your first file manually via the [Nexus Mods web UI](https://www.nexusmods.com): this creates the file that later uploads add versions to.
2. Note its file ID from the **API Info** option on the mod page's Files tab, or from the file's edit menu on the Manage Files page.
3. Create the `nexus` environment (Settings → Environments → New environment) with a required reviewer, before the workflow referencing it is merged to `main`. Otherwise GitHub auto-creates it unprotected on first reference.
4. Add to the `nexus` environment as secrets (Settings → Environments → `nexus` → Environment secrets):
   - `NEXUSMODS_API_KEY`: your Nexus Mods API key
   - `NEXUSMODS_FILE_ID`: the file ID
   - `NEXUSMODS_MOD_ID`: the mod's internal ID, used to post each release's notes to the mod's Changelog tab. **Not** the number in the mod page URL. Look that URL number up via `https://api.nexusmods.com/v3/games/skyrimspecialedition/mods/<url-id>` (needs an `apikey` header) and use the `id` field from the response.
5. Add to the `nexus` environment as a variable (Settings → Environments → `nexus` → Environment variables):
   - `NEXUSMODS_DISPLAY_NAME`: the file name shown on Nexus

## CI

| Workflow           | Trigger                                                                                                                                                                                                                              | What it does                                                                                                                                |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------- |
| `ci.yml`           | PRs to `main` touching `src/`, `test/`, `.clang-format`, `.clang-tidy`, `cmake/`, `vcpkg.json`, `.gitmodules`, the `lib/` submodule pins, `CMakeLists.txt`, `CMakePresets.json`, or `ci.yml` itself; also manual `workflow_dispatch` | `clang-format` (ubuntu) → `test` + `build` (windows, parallel) → `clang-tidy` (windows)                                                     |
| `release.yml`      | Manual `workflow_dispatch` (see [Making a release](#making-a-release) and [Test builds](#test-builds))                                                                                                                               | Computes the version, builds, packages via `scripts/package.sh`, tags the commit, publishes a GitHub Release (or prerelease) with zip + PDB |
| `nexus-upload.yml` | Auto-triggered via `workflow_run` once `release.yml` completes, gated on approval in the `nexus` environment; also manual `workflow_dispatch` (see [Nexus Mods upload](#nexus-mods-upload))                                          | Downloads release zip, generates cliff release notes, uploads to Nexus Mods                                                                 |
| `lint.yml`         | PRs touching `scripts/`                                                                                                                                                                                                              | Runs shellcheck on shell scripts                                                                                                            |
| `pr-title.yml`     | PR opened/edited/reopened/synchronize                                                                                                                                                                                                | Checks PR title follows Conventional Commits (`feat`, `fix`, `chore`, `refactor`)                                                           |
