#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "Usage: ./release.sh <version>"
    echo "Example: ./release.sh 1.0.4"
    exit 1
fi

VERSION="$1"
TAG="v$VERSION"
CHANGELOG="CHANGELOG.md"

# ---------------------------------------------------------------------------
# Basic checks
# ---------------------------------------------------------------------------

if ! [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: invalid version '$VERSION'. Expected X.Y.Z."
    exit 1
fi

if [ "$(git branch --show-current)" != "main" ]; then
    echo "Error: releases must be created from main."
    exit 1
fi

if [ -n "$(git status --porcelain)" ]; then
    echo "Error: working tree is not clean."
    echo
    git status --short
    exit 1
fi

git fetch origin main --quiet

if ! git merge-base --is-ancestor origin/main HEAD; then
    echo "Error: local main is behind or has diverged from origin/main."
    exit 1
fi

if git rev-parse "$TAG" >/dev/null 2>&1; then
    echo "Error: tag $TAG already exists locally."
    exit 1
fi

if git ls-remote --exit-code --tags origin "refs/tags/$TAG" >/dev/null 2>&1; then
    echo "Error: tag $TAG already exists on origin."
    exit 1
fi

# ---------------------------------------------------------------------------
# Changelog
# ---------------------------------------------------------------------------

if [ ! -f "$CHANGELOG" ]; then
    echo "Error: $CHANGELOG does not exist."
    exit 1
fi

NOTES="$(
    awk -v version="$VERSION" '
        $0 ~ "^## " version "([[:space:]]|—)" {
            found = 1
            next
        }

        found && /^## / {
            exit
        }

        found {
            print
        }
    ' "$CHANGELOG"
)"

if [ -z "$(printf '%s' "$NOTES" | tr -d '[:space:]')" ]; then
    echo "Error: no changelog entry found for version $VERSION."
    echo
    echo "Expected a section such as:"
    echo "## $VERSION — YYYY-MM-DD"
    exit 1
fi

# Require actual release content, not just an empty heading.
if ! printf '%s\n' "$NOTES" | grep -Eq '^[[:space:]]*[-*] '; then
    echo "Error: changelog section for $VERSION contains no list entry."
    exit 1
fi

# ---------------------------------------------------------------------------
# Preview
# ---------------------------------------------------------------------------

echo
echo "Release $TAG"
echo "------------------------------------------------------------------------"
printf '%s\n' "$NOTES"
echo "------------------------------------------------------------------------"
echo

read -r -p "Create and push $TAG? [y/N] " ANSWER

case "$ANSWER" in
    y|Y|yes|YES)
        ;;
    *)
        echo "Release cancelled."
        exit 0
        ;;
esac

# ---------------------------------------------------------------------------
# Publish
# ---------------------------------------------------------------------------

git push origin main

git tag -a "$TAG" -m "FileSpacer $VERSION"
git push origin "$TAG"

echo
echo "$TAG pushed successfully."
echo "GitHub Actions will now build and publish FileSpacer $VERSION."