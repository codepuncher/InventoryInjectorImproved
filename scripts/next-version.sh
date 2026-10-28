#!/usr/bin/env bash
# next-version.sh: print the version the next release gets, from the latest vX.Y.Z tag.

set -euo pipefail

USAGE='Usage: ./scripts/next-version.sh <major|minor|patch|X.Y.Z>'
RELEASE_TAG='^v[0-9]+\.[0-9]+\.[0-9]+$'

if [[ $# -ne 1 ]]; then
    echo "${USAGE}" >&2
    exit 1
fi

LATEST=$(git tag --list | grep -E "${RELEASE_TAG}" | sort -V | tail -1 | sed 's/^v//' || true)

case "${1}" in
    major | minor | patch)
        if [[ -z "${LATEST}" ]]; then
            echo 'Error: no vX.Y.Z release tags to bump from; give an explicit version' >&2
            exit 1
        fi
        IFS='.' read -r MAJOR MINOR PATCH <<<"${LATEST}"
        case "${1}" in
            major) TARGET="$((MAJOR + 1)).0.0" ;;
            minor) TARGET="${MAJOR}.$((MINOR + 1)).0" ;;
            patch) TARGET="${MAJOR}.${MINOR}.$((PATCH + 1))" ;;
        esac
        ;;
    *)
        TARGET="${1}"
        ;;
esac

if [[ ! "${TARGET}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: invalid version '${TARGET}', expected major, minor, patch or X.Y.Z" >&2
    exit 1
fi

LOWEST=$(printf '%s\n' "${LATEST:-0.0.0}" "${TARGET}" | sort -V | head -1)
if [[ -n "${LATEST}" && "${LOWEST}" == "${TARGET}" ]]; then
    echo "Error: ${TARGET} is not newer than the latest release ${LATEST}" >&2
    exit 1
fi

if git rev-parse -q --verify "refs/tags/v${TARGET}" >/dev/null; then
    echo "Error: tag v${TARGET} already exists" >&2
    exit 1
fi

HEAD_TAG=$(git tag --points-at HEAD | grep -E "${RELEASE_TAG}" | head -1 || true)
if [[ -n "${HEAD_TAG}" ]]; then
    echo "Error: this commit is already released as ${HEAD_TAG}" >&2
    exit 1
fi

echo "${TARGET}"
