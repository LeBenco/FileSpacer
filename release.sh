#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: ./release.sh <version> [--distribute winget|chocolatey|none]..."
    echo "Default: GitHub release only (none). Repeat --distribute to select several channels."
    echo "Example: ./release.sh 1.3.3 --distribute winget --distribute chocolatey"
}

if [ "$#" -eq 1 ] && [[ "$1" == --help || "$1" == -h ]]; then
    usage
    exit 0
fi
if [ "$#" -lt 1 ]; then
    usage
    exit 1
fi

VERSION="$1"
shift
DISTRIBUTION=none
EXPLICIT_NONE=false
while [ "$#" -gt 0 ]; do
    if [ "$1" != --distribute ] || [ "$#" -lt 2 ]; then
        usage >&2
        exit 1
    fi
    case "$2" in
        none)
            if [ "$DISTRIBUTION" != none ]; then
                echo "Error: none cannot be combined with a distribution channel." >&2
                exit 1
            fi
            EXPLICIT_NONE=true
            ;;
        winget|chocolatey)
            if [ "$EXPLICIT_NONE" = true ]; then
                echo "Error: none cannot be combined with a distribution channel." >&2
                exit 1
            fi
            if [ "$DISTRIBUTION" = none ]; then
                DISTRIBUTION="$2"
            elif [[ ",$DISTRIBUTION," != *",$2,"* ]]; then
                DISTRIBUTION+=",$2"
            fi
            ;;
        *)
            echo "Error: unknown distribution channel '$2'. Expected winget, chocolatey or none." >&2
            exit 1
            ;;
    esac
    shift 2
done
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
echo "Distribution: $DISTRIBUTION"
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

git tag -a "$TAG" -m "FileSpacer $VERSION" -m "FileSpacer-Distribution: $DISTRIBUTION"
git push origin "$TAG"

echo
echo "$TAG pushed successfully."
echo "GitHub Actions will now build and publish FileSpacer $VERSION."
