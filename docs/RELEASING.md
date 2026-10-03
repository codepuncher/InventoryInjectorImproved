# Releasing

## Versioning

The version lives in two places: `CMakeLists.txt` (`project(... VERSION x.y.z)`) and `vcpkg.json` (`version-string`). Bump both together.

## Cutting a release

1. Run `./scripts/release.sh <major|minor|patch|X.Y.Z>` on `main`. It bumps
   `CMakeLists.txt` and `vcpkg.json` on a `chore/release-X.Y.Z` branch, opens
   a PR (labeled `ignore-for-release`), waits for its checks, squash-merges
   it, then tags and pushes `vX.Y.Z` — which triggers `release.yml` to build
   the plugin, package it via `scripts/package.sh`, and publish a GitHub
   Release with the zip and PDB. The `CMakeLists.txt` version is also
   embedded in the DLL and read by the co-save validity check, and
   `release.yml` names the zip after the tag, so keeping all three
   (files, tag, DLL) in lockstep matters. Requires the GitHub CLI (`gh`),
   authenticated with push/merge access.
2. `nexus-upload.yml` auto-triggers once `release.yml` finishes. Approve the pending deployment under the `nexus` environment (on the workflow run's page, or the repo's Environments tab) to let the upload proceed. If it doesn't fire, or you need to re-run a failed upload, trigger it manually via workflow_dispatch with the version (no `v` prefix); it must match the tag exactly, since the workflow resolves the tag from the checked-out commit and fails outright if that tag doesn't exist. See [Nexus Mods upload](#nexus-mods-upload) for the environment setup.
3. If the Nexus page changed, regenerate it (see below) and paste it into the Nexus Mods page editor.

## Nexus Mods page

The `<!-- nexus:start/end -->` block at the top of [README.md](../README.md) is the **source of truth** for Requirements, Installation, and Compatibility. [nexus-page.md](nexus-page.md) holds the rest of the page (short description, overview with its menus-covered list, console commands, FAQ, credits).

To update the Nexus page:

1. Edit Requirements/Installation/Compatibility inside the `<!-- nexus:start/end -->` block in [README.md](../README.md).
2. Edit the short description, overview, console commands, FAQ, and credits directly in [nexus-page.md](nexus-page.md).
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

| Workflow | Trigger | What it does |
|---|---|---|
| `ci.yml` | PRs to `main` touching `src/`, `test/`, `.clang-format`, `.clang-tidy`, `cmake/`, `vcpkg.json`, `.gitmodules`, the `lib/` submodule pins, `CMakeLists.txt`, `CMakePresets.json`, or `ci.yml` itself; also manual `workflow_dispatch` | `clang-format` (ubuntu) → `test` + `build` (windows, parallel) → `clang-tidy` (windows) |
| `release.yml` | Push of a `v*` tag | Builds, packages via `scripts/package.sh`, publishes a GitHub Release with zip + PDB |
| `nexus-upload.yml` | Auto-triggered via `workflow_run` once `release.yml` completes, gated on approval in the `nexus` environment; also manual `workflow_dispatch` (see [Nexus Mods upload](#nexus-mods-upload)) | Downloads release zip, generates cliff release notes, uploads to Nexus Mods |
| `lint.yml` | PRs touching `scripts/` | Runs shellcheck on shell scripts |
| `pr-title.yml` | PR opened/edited/reopened/synchronize | Checks PR title follows Conventional Commits (`feat`, `fix`, `chore`, `refactor`) |
