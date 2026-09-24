#!/usr/bin/env bash
# update.sh — update the CommonLibSSE-NG submodule to the latest commit on the ng branch.
#
# Usage: ./scripts/update.sh

set -euo pipefail

if [[ ! -f "CMakeLists.txt" ]]; then
	echo "Error: must be run from the project root (e.g. ./scripts/update.sh)"
	exit 1
fi

echo "Updating CommonLibSSE-NG submodule..."
git submodule update --remote lib/commonlibsse-ng

echo ""
echo "Submodule updated. Next steps:"
echo "  git add lib/commonlibsse-ng"
echo "  git commit -m 'chore: update CommonLibSSE-NG submodule'"
