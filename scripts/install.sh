#!/bin/sh
# Interactive, user-local PIPI installer for Linux and Termux.
# Never removes or overwrites files under PIPI_HOME. Never writes to system PATH.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=$(cat "$ROOT/PIPI_VERSION" 2>/dev/null || echo unknown)

say() { printf '%s\n' "$*"; }
fail() { say "[PIPI installer] ERROR: $*" >&2; exit 1; }

TARGET=${PIPI_INSTALL_TARGET:-}
if [ -z "$TARGET" ]; then
  say ""
  say "PIPI v$VERSION — choose the OS for this installation"
  say "  1) Termux / Android"
  say "  2) Linux"
  say "  3) Windows (use install.bat on Windows)"
  printf 'Select [1-3]: '
  IFS= read -r CHOICE || exit 2
  case "$CHOICE" in
    1) TARGET=termux ;;
    2) TARGET=linux ;;
    3) say "Run install.bat from the extracted PIPI folder on Windows."; exit 0 ;;
    *) fail "invalid selection" ;;
  esac
fi

OS=$(uname -s 2>/dev/null || echo unknown)
IS_TERMUX=0
case "${PREFIX:-}" in *com.termux*|*/com.termux/files/usr) IS_TERMUX=1 ;; esac
if [ -n "${TERMUX_VERSION:-}" ]; then IS_TERMUX=1; fi

case "$TARGET" in
  termux)
    [ "$OS" = Linux ] || fail "Termux install selected, but this is not a Linux/Android shell. Run this script inside Termux."
    [ "$IS_TERMUX" -eq 1 ] || fail "Termux target selected but PREFIX/TERMUX_VERSION does not identify Termux. Run inside Termux."
    [ -n "${PREFIX:-}" ] || fail "Termux PREFIX is missing. Open the Termux shell and run the installer there."
    command -v clang >/dev/null 2>&1 || fail "Clang is required. Run: pkg install clang"
    CC=${CC:-clang}
    TOOLS_HOME=${PIPI_TOOLS_HOME:-$PREFIX/opt/pipi}
    COMMAND_DIR=${PIPI_COMMAND_DIR:-$PREFIX/bin}
    PROFILE_FILE=${PIPI_PROFILE_FILE:-$HOME/.bashrc}
    ;;
  linux)
    [ "$OS" = Linux ] || fail "Linux target selected, but uname reports '$OS'."
    [ "$IS_TERMUX" -eq 0 ] || fail "This shell is Termux. Choose option 1 for Termux / Android."
    CC=${CC:-cc}
    command -v "$CC" >/dev/null 2>&1 || fail "C compiler '$CC' not found. Install GCC or Clang first."
    TOOLS_HOME=${PIPI_TOOLS_HOME:-$HOME/.local/opt/pipi}
    COMMAND_DIR=${PIPI_COMMAND_DIR:-$HOME/.local/bin}
    PROFILE_FILE=${PIPI_PROFILE_FILE:-$HOME/.profile}
    ;;
  windows)
    say "Run scripts\\install.bat from Windows CMD or PowerShell."; exit 0 ;;
  *) fail "unknown target '$TARGET' (expected termux or linux)" ;;
esac

[ -n "${HOME:-}" ] || fail 'HOME is not set'
PIPI_HOME=${PIPI_HOME:-$HOME/.pipi}
TOOLS_BIN=$TOOLS_HOME/bin
mkdir -p "$TOOLS_BIN" "$COMMAND_DIR" "$PIPI_HOME"

say ""
say "[PIPI] Building v$VERSION for $TARGET on $(uname -m)..."
if [ "$TARGET" = termux ]; then
  CC="$CC" "$ROOT/build.sh"
else
  CC="$CC" "$ROOT/build.sh"
fi

for TOOL in pipi-c pipi-vm pipiwood; do
  [ -x "$ROOT/bin/$TOOL" ] || fail "build did not produce bin/$TOOL"
  TMP="$TOOLS_BIN/.$TOOL.tmp.$$"
  install -m 755 "$ROOT/bin/$TOOL" "$TMP"
  mv -f "$TMP" "$TOOLS_BIN/$TOOL"
done

# Install the optional platform GUI bridge beside PIPI's private tools. The VM
# uses PIPI_GUI_LIBRARY; this avoids relying on the caller's working directory.
GUI_LIBRARY_PATH=""
case "$(uname -m)" in
  x86_64|amd64) PLATFORM=linux-x86_64 ;;
  aarch64|arm64) PLATFORM=linux-aarch64 ;;
  armv7l|armv7*) PLATFORM=linux-armv7 ;;
  i?86) PLATFORM=linux-x86 ;;
  *) PLATFORM=linux-unknown ;;
esac
GUI_SOURCE=$ROOT/native/$PLATFORM/libpipi_gui.so
if [ -f "$GUI_SOURCE" ]; then
  GUI_DEST=$TOOLS_HOME/native/$PLATFORM/libpipi_gui.so
  mkdir -p "$(dirname "$GUI_DEST")"
  GUI_TMP=$GUI_DEST.tmp.$$
  install -m 755 "$GUI_SOURCE" "$GUI_TMP"
  mv -f "$GUI_TMP" "$GUI_DEST"
  GUI_LIBRARY_PATH=$GUI_DEST
fi

link_tool() {
  TOOL=$1
  SOURCE=$TOOLS_BIN/$TOOL
  DEST=$COMMAND_DIR/$TOOL
  if [ -L "$DEST" ]; then
    OLD_TARGET=$(readlink "$DEST" 2>/dev/null || true)
    case "$OLD_TARGET" in
      "$SOURCE") return 0 ;;
      */opt/pipi/bin/"$TOOL"|*/PIPI/bin/"$TOOL") rm -f "$DEST" ;;
      *) say "[PIPI] Keeping existing symlink $DEST -> $OLD_TARGET (not owned by this installer)."; return 1 ;;
    esac
  elif [ -e "$DEST" ]; then
    OLD_VERSION=$("$DEST" --version 2>/dev/null || true)
    case "$OLD_VERSION" in
      PIPI*)
        BACKUP_DIR=$TOOLS_HOME/backups
        mkdir -p "$BACKUP_DIR"
        BACKUP=$BACKUP_DIR/$TOOL.pre-installer.$VERSION
        if [ -e "$BACKUP" ]; then BACKUP=$BACKUP_DIR/$TOOL.pre-installer.$VERSION.$(date +%s); fi
        mv "$DEST" "$BACKUP"
        say "[PIPI] Backed up previous PIPI command: $BACKUP"
        ;;
      *)
        say "[PIPI] An unrelated or unrecognized command already exists: $DEST"
        say "       It will NOT be overwritten automatically."
        printf 'Back it up and replace it with PIPI? [y/N]: '
        ANSWER=n
        if [ -t 0 ]; then
          if read -r ANSWER; then :; fi
        fi
        case "$ANSWER" in
          y|Y|yes|YES)
            BACKUP_DIR=$TOOLS_HOME/backups
            mkdir -p "$BACKUP_DIR"
            BACKUP=$BACKUP_DIR/$TOOL.manual-backup.$VERSION
            if [ -e "$BACKUP" ]; then BACKUP=$BACKUP_DIR/$TOOL.manual-backup.$VERSION.$(date +%s); fi
            mv "$DEST" "$BACKUP"
            say "[PIPI] Existing command backed up to: $BACKUP"
            ;;
          *)
            say "[PIPI] Keeping $DEST. New PIPI binary is available at: $SOURCE"
            return 1
            ;;
        esac
        ;;
    esac
  fi
  ln -s "$SOURCE" "$DEST"
  return 0
}

LINKED=0
for TOOL in pipi-c pipi-vm pipiwood; do
  if link_tool "$TOOL"; then LINKED=$((LINKED + 1)); fi
done

# Add the command directory only if it is not already on PATH. A small managed
# block is appended; existing shell configuration is never replaced or edited.
case ":${PATH:-}:" in
  *":$COMMAND_DIR:"*) ;;
  *)
    if ! grep -F '>>> PIPI managed PATH' "$PROFILE_FILE" >/dev/null 2>&1; then
      mkdir -p "$(dirname "$PROFILE_FILE")"
      {
        say ''
        say '# >>> PIPI managed PATH (do not duplicate) >>>'
        say "case \":\$PATH:\" in *\":$COMMAND_DIR:\"*) ;; *) PATH=\"$COMMAND_DIR:\$PATH\" ;; esac"
        say 'export PATH'
        say '# <<< PIPI managed PATH <<<'
      } >> "$PROFILE_FILE"
      say "[PIPI] Added a managed PATH block to $PROFILE_FILE"
    else
      say "[PIPI] PATH block already exists in $PROFILE_FILE"
    fi
    ;;
esac

if [ -n "$GUI_LIBRARY_PATH" ] && [ -z "${PIPI_GUI_LIBRARY:-}" ]; then
  if ! grep -F '>>> PIPI managed GUI' "$PROFILE_FILE" >/dev/null 2>&1; then
    {
      say ''
      say '# >>> PIPI managed GUI library >>>'
      say "if [ -z \"\${PIPI_GUI_LIBRARY:-}\" ]; then PIPI_GUI_LIBRARY=\"$GUI_LIBRARY_PATH\"; fi"
      say 'export PIPI_GUI_LIBRARY'
      say '# <<< PIPI managed GUI library <<<'
    } >> "$PROFILE_FILE"
  fi
fi

say ""
say "[PIPI] Installing standard .woodlib packages into: $PIPI_HOME"
FOUND=0
INSTALLED=0
SKIPPED=0
for ARCHIVE in "$ROOT"/dist/woodlibs/pipi_*.woodlib; do
  [ -f "$ARCHIVE" ] || continue
  FOUND=$((FOUND + 1))
  ID=$(basename "$ARCHIVE" .woodlib)
  MOD=${ID#pipi_}
  MARKER=$PIPI_HOME/.installed/$ID.files
  MODULE_DIR=$PIPI_HOME/modules/pipi/$MOD
  if [ -f "$MARKER" ] || [ -d "$MODULE_DIR" ]; then
    say "[PIPI] Preserving existing module/package: pipi/$MOD (not overwritten)"
    SKIPPED=$((SKIPPED + 1))
    continue
  fi
  "$TOOLS_BIN/pipiwood" verify "$ARCHIVE" >/dev/null
  "$TOOLS_BIN/pipiwood" install "$ARCHIVE" --root "$PIPI_HOME"
  INSTALLED=$((INSTALLED + 1))
done
[ "$FOUND" -gt 0 ] || fail 'no dist/woodlibs/pipi_*.woodlib packages were produced by the build'

say ""
say "[PIPI] Verifying installed command and standard library..."
"$TOOLS_BIN/pipi-c" --version
"$TOOLS_BIN/pipi-vm" --version
"$TOOLS_BIN/pipiwood" --version
PIPI_HOME="$PIPI_HOME" "$TOOLS_BIN/pipi-c" check "$ROOT/examples/stdlib_demo.pipi"

say ""
say "[PIPI] Installation complete."
say "  Tools:       $TOOLS_BIN"
say "  PATH links:  $COMMAND_DIR ($LINKED/3 commands linked)"
say "  Modules:     $PIPI_HOME ($INSTALLED installed, $SKIPPED preserved)"
if [ -n "$GUI_LIBRARY_PATH" ]; then say "  GUI bridge:  $GUI_LIBRARY_PATH"; fi
if [ "$LINKED" -lt 3 ]; then
  say "Some command names were already occupied. Existing commands were preserved."
  say "Run PIPI directly from: $TOOLS_BIN/pipi-c, $TOOLS_BIN/pipi-vm, $TOOLS_BIN/pipiwood"
fi
case ":${PATH:-}:" in *":$COMMAND_DIR:"*) ;; *) say "Open a new shell or run: export PATH=\"$COMMAND_DIR:\$PATH\"" ;; esac
say "Your existing modules were not deleted or overwritten."
