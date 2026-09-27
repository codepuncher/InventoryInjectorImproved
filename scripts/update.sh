#!/usr/bin/env bash
# update.sh: pin the CommonLibSSE-NG submodule to a release tag.
#
# Usage: ./scripts/update.sh [tag]    (default: the latest v* release tag)

set -euo pipefail

if [[ ! -f "CMakeLists.txt" ]]; then
	echo "Error: must be run from the project root (e.g. ./scripts/update.sh)"
	exit 1
fi

submodule="lib/commonlibsse-ng"

if [[ ! -e "${submodule}/.git" ]]; then
	echo "Error: ${submodule} is not initialised (run: git submodule update --init)"
	exit 1
fi

echo "Fetching CommonLibSSE-NG tags..."
git -C "${submodule}" fetch --tags origin ng

tag="${1:-$(git -C "${submodule}" for-each-ref --count=1 --sort=-v:refname --merged origin/ng --format='%(refname:short)' 'refs/tags/v[0-9]*')}"
if [[ -z "${tag}" ]]; then
	echo "Error: no release tag found on origin/ng in ${submodule}"
	exit 1
fi
if ! git -C "${submodule}" rev-parse --verify --quiet "refs/tags/${tag}" >/dev/null; then
	echo "Error: tag '${tag}' not found in ${submodule}"
	exit 1
fi

echo "Checking out CommonLibSSE-NG ${tag}..."
git -C "${submodule}" checkout --quiet "refs/tags/${tag}"
git -C "${submodule}" submodule update --init

# CMake configure runs `git submodule update`, which resets an unstaged pin.
git add "${submodule}"

echo ""
echo "Submodule pinned to ${tag} and staged. Next step:"
echo "  git commit -m 'chore(deps): update CommonLibSSE-NG to ${tag}'"
