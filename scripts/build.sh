#!/usr/bin/env bash
# build.sh: configure and build the mod for Linux (clang-cl cross-compilation).
#
# Usage: ./scripts/build.sh

set -euo pipefail

if [[ ! -f "CMakeLists.txt" ]]; then
    echo "Error: must be run from the project root (e.g. ./scripts/build.sh)"
    exit 1
fi

if [[ -f ".env" ]]; then
    set -a
    # shellcheck source=/dev/null
    source .env
    set +a
fi

echo "Configuring..."
cmake --preset release-linux

echo "Building..."
cmake --build --preset release-linux

echo ""
echo "Build complete"
