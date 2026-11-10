#!/bin/sh
# One-time GitHub repository setup for PIPI.
# Safe to run again: an existing origin is reused; this script never renames it.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

REPO=${1:-}
VISIBILITY=${2:---public}

usage() {
  cat <<'USAGE'
Usage:
  ./scripts/github-init.sh OWNER/REPOSITORY [--public|--private]

Run once per GitHub repository. Requires git, GitHub CLI (gh), and `gh auth login`.
If origin already points to OWNER/REPOSITORY, it is reused and never recreated.
USAGE
}

if [ -z "$REPO" ]; then usage >&2; exit 2; fi
case "$REPO" in
  */*) ;;
  *) echo "error: repository must be OWNER/REPOSITORY" >&2; usage >&2; exit 2 ;;
esac
case "$REPO" in
  *..*|*' '*|*\|*|*\\*) echo "error: invalid repository name: $REPO" >&2; exit 2 ;;
esac
case "$VISIBILITY" in
  --public|--private) ;;
  *) echo "error: visibility must be --public or --private" >&2; exit 2 ;;
esac

command -v git >/dev/null 2>&1 || { echo "error: git is required" >&2; exit 127; }

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  git init
fi

ORIGIN=$(git config --get remote.origin.url 2>/dev/null || true)
GITHUB_URL="https://github.com/$REPO.git"
if [ -n "$ORIGIN" ]; then
  case "$ORIGIN" in
    *"$REPO"*)
      echo "[PIPI] Reusing existing origin: $ORIGIN"
      ;;
    *)
      echo "error: origin already points somewhere else: $ORIGIN" >&2
      echo "Refusing to replace it or create another repository." >&2
      exit 1
      ;;
  esac
else
  command -v gh >/dev/null 2>&1 || { echo "error: GitHub CLI (gh) is required to create the remote repository" >&2; exit 127; }
  gh auth status >/dev/null 2>&1 || {
    echo "error: GitHub CLI is not authenticated. Run: gh auth login" >&2
    exit 1
  }
  if gh repo view "$REPO" >/dev/null 2>&1; then
    echo "[PIPI] GitHub repository already exists; not creating it again."
  else
    echo "[PIPI] Creating GitHub repository once: $REPO ($VISIBILITY)"
    gh repo create "$REPO" "$VISIBILITY"
  fi
  git remote add origin "$GITHUB_URL"
fi

git branch -M main

# Do not accidentally push an unrelated initial history over a non-empty remote.
if ! git rev-parse --verify HEAD >/dev/null 2>&1; then
  if git ls-remote --exit-code --heads origin main >/dev/null 2>&1; then
    echo "error: origin/main already has commits but this folder has no Git history." >&2
    echo "Clone the repository first, then copy the PIPI files into that clone." >&2
    exit 1
  fi
fi

if ! git config user.name >/dev/null 2>&1 || ! git config user.email >/dev/null 2>&1; then
  echo "error: configure your Git identity first:" >&2
  echo '  git config --global user.name "Your Name"' >&2
  echo '  git config --global user.email "you@example.com"' >&2
  exit 1
fi

VERSION=$(cat PIPI_VERSION 2>/dev/null || echo unknown)
git add -A
if ! git diff --cached --quiet; then
  if git rev-parse --verify HEAD >/dev/null 2>&1; then
    git commit -m "PIPI v$VERSION: repository setup"
  else
    git commit -m "Initial commit: PIPI v$VERSION"
  fi
fi

git push -u origin main
printf '\n[PIPI] GitHub repository is connected: %s\n' "$REPO"
echo '[PIPI] Setup is complete. For every later version, run:'
echo '       ./scripts/github-publish.sh'
echo '[PIPI] This setup script does not need to be run again.'
