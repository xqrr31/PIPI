# Publishing PIPI to GitHub

PIPI has a **one-time repository setup** and a separate **repeatable publisher**.
The repository is created once. Normal version updates only commit and push to
the existing `origin`; they never call `gh repo create`.

## Requirements

- Git
- GitHub CLI (`gh`)
- A GitHub account authenticated in the CLI (`gh auth login`)

On Termux, install the tools with:

```sh
pkg install git gh
gh auth login
```

On Linux, install Git and GitHub CLI from their official packages. On Windows,
run the scripts from Git Bash with Git and GitHub CLI installed.

## 1. One-time setup — run only once

From the PIPI project root:

```sh
./scripts/github-init.sh OWNER/REPOSITORY --public
```

For a private repository, use `--private` instead. Example:

```sh
./scripts/github-init.sh my-account/pipi --public
```

What it does:

1. Checks GitHub CLI authentication.
2. Initializes Git only when this folder is not already a repository.
3. Reuses a matching `origin` if one is already configured.
4. Checks whether `OWNER/REPOSITORY` already exists on GitHub.
5. Creates the GitHub repository only if it does not exist.
6. Adds `origin`, commits the project if needed, and pushes `main`.

It refuses to replace an `origin` that points to a different repository. If the
remote repository already contains commits but this local folder has no Git
history, it stops rather than overwriting unrelated history. In that case,
clone the remote first and copy the project into that clone.

## 2. Every later version — use this script

Update `PIPI_VERSION`, `CHANGELOG.md`, and the source/docs; then build and test:

```sh
# Example for the next patch/minor release:
# edit PIPI_VERSION to 1.1.0 and add its CHANGELOG.md entry
CC=clang ./build.sh
python3 -m unittest discover -s tests -v
```

Push the update and create the immutable `v1.0.1` Git tag:

```sh
./scripts/github-publish.sh
```

Or provide a custom commit message:

```sh
./scripts/github-publish.sh --message "PIPI 1.1.0: add module improvements"
```

To also create a GitHub Release and attach any host-built packages from
`dist/woodlibs/*.woodlib`:

```sh
./scripts/github-publish.sh --release
```

The release operation uses GitHub's generated release notes. If the release
already exists, it is left unchanged.

## Safety rules

- `github-init.sh` is idempotent: it checks for the repo and reuses `origin`.
- `github-publish.sh` never creates, deletes, renames, or changes a GitHub repo.
- Version tags are immutable. If `vVERSION` already points to an older commit,
  publishing stops and asks for a version bump instead of force-moving the tag.
- Build outputs, native shared libraries, bytecode files, and `.woodlib`
  packages are ignored by Git. `--release` attaches the `.woodlib` packages
  built for the current host, if present.
- Never put GitHub tokens or passwords in scripts, source files, or commits.

## Useful checks

```sh
git remote -v
git status --short
git tag --list 'v*'
gh repo view
```
