# Contributing

## Prerequisites

### All platforms
- [Git](https://git-scm.com/)
- [CMake](https://cmake.org/download/) 3.21+
- [vcpkg](https://vcpkg.io/en/getting-started): ships as the `lib/vcpkg` submodule (cloned by `--recurse-submodules`)

### Linux
- LLVM/Clang (provides `clang-cl`, `lld-link`, `llvm-lib`, `llvm-rc`, `llvm-mt`)
- [xwin](https://github.com/Jake-Shadle/xwin): downloads the real Windows SDK and MSVC CRT headers/libs
- [Ninja](https://ninja-build.org/)

```bash
# Arch / CachyOS
sudo pacman -S clang lld llvm ninja

# Install xwin (requires Rust/cargo)
cargo install xwin

# Fetch Windows SDK + MSVC CRT headers to ~/.xwin  (one-time, ~700 MB)
xwin splat --output ~/.xwin
```

> **Note:** On first configure, `cmake/toolchains/clang-cl-cross.cmake` creates
> TitleCase symlinks inside your xwin installation, e.g.:
> ```
> ~/.xwin/sdk/lib/um/x86_64/Advapi32.lib  ->  advapi32.lib
> ```
> lld-link is case-sensitive but CommonLibSSE-NG references libs with mixed-case names.
> The originals are untouched.

### Windows
- [Visual Studio 2022](https://visualstudio.microsoft.com/) with **Desktop development with C++**

## Getting started

### Linux

#### 1. Clone

```bash
git clone --recurse-submodules https://github.com/codepuncher/InventoryInjectorImproved.git
cd InventoryInjectorImproved
```

#### 2. Configure deploy path

Copy `.env.example` to `.env` and set `SKYRIM_MODS_FOLDER` to your mod manager's staging folder:

```bash
# Vortex (Linux, Steam):
export SKYRIM_MODS_FOLDER="$HOME/.local/share/Steam/steamapps/common/Vortex Mods/skyrimse"

# MO2 (Linux):
# export SKYRIM_MODS_FOLDER="$HOME/MO2/mods"
```

#### 3. Build

```bash
./scripts/build.sh
# or directly:
cmake --preset release-linux
cmake --build --preset release-linux
```

The DLL lands in `build/release-linux/InventoryInjectorImproved.dll`.

#### Deploy to mod manager

```bash
./scripts/deploy.sh [--probe]  # --probe: see "Diagnostic builds" below
# or directly (deploy.sh sources .env for you; cmake does not):
source .env && cmake --workflow --preset deploy
```

Copies `InventoryInjectorImproved.dll` and `InventoryInjectorImproved.pdb` into `$SKYRIM_MODS_FOLDER/InventoryInjectorImproved/SKSE/Plugins/`. Vortex detects the new mod folder automatically: enable it, then launch Skyrim.

### Windows (MSVC)

```bash
cmake --preset release-windows
cmake --build --preset release-windows
```

The DLL lands in `build/msvc/InventoryInjectorImproved.dll`.

## Running tests

Unit tests cover the pure-logic headers in `src/`. Run them natively on Linux
against the system Catch2 (v3 required). No cross-compile needed:

```bash
cmake --preset test-linux
cmake --build --preset test-linux
ctest --preset test-linux
```

On Windows (MSVC), and in CI, use the `test-windows` preset instead.

Tests live in `test/` and use [Catch2](https://github.com/catchorg/Catch2). Only pure-logic code (no RE::/SKSE:: APIs) can be tested this way. See `src/CacheKey.h` and `test/CacheKeyTests.cpp` for the pattern.

## Git hooks (Lefthook)

Prerequisites:

- `go install github.com/evilmartians/lefthook@latest`
- `clang-format` (part of LLVM, already required for development)
- `clang-tidy` (part of LLVM, already required for development)
- `cmake-format` (`sudo pacman -S cmake-format` on Arch/CachyOS; `pip install cmakelang` elsewhere)
- `shellcheck` (`sudo pacman -S shellcheck` on Arch/CachyOS)

```bash
lefthook install
```

## Editor setup (clangd / Neovim)

CMake writes `compile_commands.json` to the build directory automatically.
Copy or symlink it to the project root so clangd picks it up:

```bash
# After configuring:
ln -sf build/release-linux/compile_commands.json compile_commands.json
```

The `.clangd` file already sets `--target=x86_64-pc-windows-msvc` so clangd
resolves Windows headers correctly on Linux.

Recommended Neovim plugins: [nvim-lspconfig](https://github.com/neovim/nvim-lspconfig)
with `clangd`, and [clangd_extensions.nvim](https://github.com/p00f/clangd_extensions.nvim).

## Updating CommonLibSSE-NG

```bash
git submodule update --remote lib/commonlibsse-ng
git add lib/commonlibsse-ng
git commit -m "chore(deps): update CommonLibSSE-NG submodule"
```

## Diagnostic console commands

These are developer diagnostics, not needed for normal play:

- `i5 bypass on|off`: bypasses the cache entirely, serving every item straight from I4 (raw I4, not cached). Use it to compare cached vs. uncached behavior.
- `i5 verify on|off`: turns on an assertion pass that checks the `InvalidateListData` O(N+C) fast path produces the same flags as vanilla, logging any mismatch. Off uses the fast path without the check.
- `i5 memo on|off`: turns the invalidate memo on or off. On, it skips a redundant itemList reprocess when nothing relevant changed; off, every `InvalidateData` call runs in full.

## Diagnostic builds

The per-frame spike probe (`src/FrameProbe.cpp`) is left out of release builds. Build, deploy,
and test it with `./scripts/deploy.sh --probe`. With `i5 debug on`, it logs per-frame spikes
after inventory refreshes and transfers to `InventoryInjectorImproved.log`. Run
`./scripts/deploy.sh` afterwards to restore the release DLL.

## Code style

This project follows the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines). Google and LLVM style guides are not used: both prohibit exceptions, which CommonLibSSE-NG relies on.

### Key rules

**Null checks**: use `if (!ptr)` / `if (ptr)`, never `== nullptr` or `!= nullptr` (ES.87).

**Memory**: no naked `new`/`delete` (R.11). No raw owning pointers (R.3). Use `std::unique_ptr` / `std::shared_ptr`.

**Control flow**: always use braces, even for single-statement bodies.

**Types and algorithms**: prefer `std::` types and algorithms over hand-rolled equivalents.

**Return values**: always check them. If a function returns `bool` or an error code, check it.

**Precompiled header**: `PCH.h` must be the first include in plugin-only `.cpp` files (`src/`). Headers compiled into both the plugin and the native test target (the pure-logic `src/*.h` files included by `test/`) must not include `PCH.h`.

**Comments**: only where a non-obvious decision needs a reason. No filler, no redundant restatements of the code.
