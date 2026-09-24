#!/usr/bin/env bash
# deploy.sh: build and deploy to the mod manager staging folder, or the
# game's Data folder when only SKYRIM_FOLDER is set.
#
# Requires SKYRIM_MODS_FOLDER (or, as a fallback, SKYRIM_FOLDER) to be set in .env or the environment.
#
# Usage: ./scripts/deploy.sh [--probe]
#   --probe  build and deploy the frame-probe diagnostic build instead of release

set -euo pipefail

usage="Usage: ./scripts/deploy.sh [--probe]"

build_preset="deploy"
build_desc="release"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            echo "$usage"
            exit 0 ;;
        --probe)
            build_preset="deploy-probe"
            build_desc="frame probe"
            shift ;;
        *)
            echo "Error: unknown option: $1" >&2
            echo "$usage" >&2
            exit 1 ;;
    esac
done

if [[ ! -f "CMakeLists.txt" ]]; then
    echo "Error: must be run from the project root (e.g. ./scripts/deploy.sh)" >&2
    exit 1
fi

if [[ -f ".env" ]]; then
    set -a
    # shellcheck source=/dev/null
    source .env
    set +a
fi

if [[ -z "${SKYRIM_MODS_FOLDER:-}" && -z "${SKYRIM_FOLDER:-}" ]]; then
    echo "Error: neither SKYRIM_MODS_FOLDER nor SKYRIM_FOLDER is set." >&2
    echo "  Set one in .env or export it before running this script." >&2
    exit 1
fi

echo "Building and deploying (${build_desc})"
cmake --workflow --preset "$build_preset"

echo ""
echo "Deploy complete (${build_desc})"
