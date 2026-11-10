#!/bin/sh
# Repeatable publisher. This script NEVER creates a GitHub repository.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

MAKE_RELEASE=0
COMMIT_MESSAGE=""
while [ "$#" -gt 0 ]; do
  case "$1" in
    --release) MAKE_RELEASE=1; shift ;;
    --message)
      [ "$#" -ge 2 ] || { echo "error: --message requires text" >&2; exit 2; }
      COMMIT_MESSAGE=$2; shift 2 ;;
    --help|-h)
      cat <<'USAGE'
Usage:
  ./scripts/github-publish.sh [--message "Commit message"] [--release]

Reads the release number from PIPI_VERSION, commits changes, pushes the current
branch and an immutable vVERSION tag to the already-configured origin.
--release additionally creates a GitHub Release using `gh` and attaches any
built dist/woodlibs/*.woodlib packages. It never creates a repository.
USAGE
      exit 0
      ;;
    *) echo "error: unknown option: $1" >&2; exit 2 ;;
  esac
done

command -v git >/dev/null 2>&1 || { echo "error: git is required" >&2; exit 127; }
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || {
  echo "error: this folder is not connected to a Git repository." >&2
  echo "Run ./scripts/github-init.sh OWNER/REPOSITORY once first." >&2
  exit 1
}
ORIGIN=$(git config --get remote.origin.url 2>/dev/null || true)
[ -n "$ORIGIN" ] || {
  echo "error: no 'origin' remote configured. Run github-init.sh once." >&2
  exit 1
}

VERSION=$(cat PIPI_VERSION 2>/dev/null || true)
case "$VERSION" in
  ''|*[!0-9.]*|.*|*..*|*.) echo "error: invalid PIPI_VERSION: $VERSION" >&2; exit 1 ;;
esac
TAG="v$VERSION"
BRANCH=$(git branch --show-current 2>/dev/null || true)
[ -n "$BRANCH" ] || { echo "error: detached HEAD; switch to a branch before publishing" >&2; exit 1; }

# Stage first so untracked files are also detected before checking immutable tags.
# Generated binaries and build output are ignored by .gitignore.
git add -A

# A version tag is immutable. Force a version bump instead of silently moving it.
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null 2>&1; then
  TAG_COMMIT=$(git rev-list -n 1 "$TAG")
  HEAD_COMMIT=$(git rev-parse HEAD)
  if [ "$TAG_COMMIT" != "$HEAD_COMMIT" ] || ! git diff --quiet || ! git diff --cached --quiet; then
    echo "error: $TAG already exists and is immutable." >&2
    echo "Bump PIPI_VERSION before publishing new changes; the script will not overwrite tags." >&2
    exit 1
  fi
  TAG_EXISTS=1
else
  TAG_EXISTS=0
fi

if [ "$TAG_EXISTS" -eq 0 ]; then
  if ! git diff --cached --quiet; then
    if [ -z "$COMMIT_MESSAGE" ]; then COMMIT_MESSAGE="PIPI v$VERSION"; fi
    git commit -m "$COMMIT_MESSAGE"
  fi
  git tag -a "$TAG" -m "PIPI v$VERSION"
fi

echo "[PIPI] Pushing branch '$BRANCH' to existing origin: $ORIGIN"
git push -u origin "$BRANCH"
git push origin "$TAG"

if [ "$MAKE_RELEASE" -eq 1 ]; then
  command -v gh >/dev/null 2>&1 || {
    echo "error: --release requires GitHub CLI (gh). Install it and run gh auth login." >&2
    exit 127
  }
  gh auth status >/dev/null 2>&1 || {
    echo "error: GitHub CLI is not authenticated. Run: gh auth login" >&2
    exit 1
  }
  if gh release view "$TAG" >/dev/null 2>&1; then
    echo "[PIPI] GitHub Release $TAG already exists; leaving it unchanged."
  else
    set --
    for ASSET in "$ROOT"/dist/woodlibs/*.woodlib; do
      [ -f "$ASSET" ] && set -- "$@" "$ASSET"
    done
    gh release create "$TAG" --title "PIPI v$VERSION" --generate-notes "$@"
  fi
fi

printf '\n[PIPI] Publish complete: branch=%s tag=%s\n' "$BRANCH" "$TAG"
if [ "$MAKE_RELEASE" -eq 0 ]; then
  echo '[PIPI] To also create a GitHub Release with .woodlib assets, run:'
  echo '       ./scripts/github-publish.sh --release'
fi
