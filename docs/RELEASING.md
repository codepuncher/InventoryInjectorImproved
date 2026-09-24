# Releasing

## Versioning

The version lives in two places: `CMakeLists.txt` (`project(... VERSION x.y.z)`) and `vcpkg.json` (`version-string`). Bump both together.

## Cutting a release

1. Bump the version in `CMakeLists.txt` and `vcpkg.json` on a branch. The `CMakeLists.txt` version is also embedded in the DLL and read by the co-save validity check, so a release without this bump lets old co-saves pass the check against a new build.
2. Merge it to `main`.
3. Tag `vX.Y.Z` on `main`, matching the bumped version exactly: `release.yml` names the zip after the tag, so a mismatch leaves the zip and Nexus file version disagreeing with the DLL's embedded version.
4. Push the tag. `release.yml` builds the plugin, packages it via `scripts/package.sh`, and publishes a GitHub Release with the zip and PDB.
5. Run `nexus-upload.yml` via workflow_dispatch, passing the version (no `v` prefix, per its input description). It must match the tag exactly: the workflow checks out and downloads `v<version>`, and fails outright if that tag doesn't exist. See [Nexus Mods upload](#nexus-mods-upload) for why this step is manual.
6. If the Nexus page changed, regenerate it (see below) and paste it into the Nexus Mods page editor.

## Nexus Mods page

The `<!-- nexus:start/end -->` block at the top of [README.md](../README.md) is the **source of truth** for Requirements, Installation, and Compatibility. [nexus-page.md](nexus-page.md) holds the rest of the page (short description, overview with its menus-covered list, console commands, FAQ, credits).

To update the Nexus page:

1. Edit Requirements/Installation/Compatibility inside the `<!-- nexus:start/end -->` block in [README.md](../README.md).
2. Edit the short description, overview, console commands, FAQ, and credits directly in [nexus-page.md](nexus-page.md).
   > **Do not edit** the `<!-- generated:start/end -->` block in [nexus-page.md](nexus-page.md): it is overwritten every time the script runs.
3. Generate the combined BBCode output:

```bash
python3 scripts/generate-nexus-page.py

# Or pipe straight to the clipboard:
python3 scripts/generate-nexus-page.py | xclip -selection clipboard  # Linux
python3 scripts/generate-nexus-page.py | pbcopy                       # macOS
```

4. Paste the output into the Nexus Mods page editor.

## Nexus Mods upload

`nexus-upload.yml` declares a `release: published` trigger, but it doesn't fire: `release.yml` publishes the GitHub Release using the default `GITHUB_TOKEN`, and GitHub doesn't run other workflows off events caused by `GITHUB_TOKEN`. Run `nexus-upload.yml` by hand via **workflow_dispatch** with the version (no `v` prefix) instead.

**Prerequisites (one-time setup):**
1. Upload your first file manually via the [Nexus Mods web UI](https://www.nexusmods.com): this creates the file that later uploads add versions to.
2. Note its file ID from the **API Info** option on the mod page's Files tab, or from the file's edit menu on the Manage Files page.
3. Add to your repository as secrets (Settings → Secrets → Actions):
   - `NEXUSMODS_API_KEY`: your Nexus Mods API key
   - `NEXUSMODS_FILE_ID`: the file ID

## CI

| Workflow | Trigger | What it does |
|---|---|---|
| `ci.yml` | PRs to `main` touching `src/`, `test/`, `.clang-format`, `.clang-tidy`, `cmake/`, `vcpkg.json`, `CMakeLists.txt`, `CMakePresets.json`, or `ci.yml` itself; also manual `workflow_dispatch` | `clang-format` (ubuntu) → `test` + `build` (windows, parallel) → `clang-tidy` (windows) |
| `release.yml` | Push of a `v*` tag | Builds, packages via `scripts/package.sh`, publishes a GitHub Release with zip + PDB |
| `nexus-upload.yml` | Manual `workflow_dispatch` (its `release: published` trigger doesn't fire, see [Nexus Mods upload](#nexus-mods-upload)) | Downloads release zip, generates cliff release notes, uploads to Nexus Mods |
| `lint.yml` | PRs touching `scripts/` | Runs shellcheck on shell scripts |
| `pr-title.yml` | PR opened/edited/reopened/synchronize | Checks PR title follows Conventional Commits (`feat`, `fix`, `chore`, `refactor`) |
