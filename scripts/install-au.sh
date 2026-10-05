#!/usr/bin/env bash
# Installs the stereophield Audio Unit from a GitHub release:
#
#   curl -fsSL https://raw.githubusercontent.com/highda/stereophield/main/scripts/install-au.sh | bash
#   curl -fsSL .../install-au.sh | bash -s -- 3.1.0      a specific version
#
# Files fetched with curl carry no quarantine flag, so neither Gatekeeper nor
# the host asks about the plugin. The archive is checked against the
# release's SHA-256 file before anything is installed.
set -euo pipefail

REPO="highda/stereophield"
VERSION="${1:-latest}"
DEST="${STEREOPHIELD_AU_DIR:-$HOME/Library/Audio/Plug-Ins/Components}"

if [[ "$(uname -s)" != "Darwin" || "$(uname -m)" != "arm64" ]]; then
  echo "stereophield runs on macOS on Apple Silicon (arm64) only." >&2
  exit 1
fi

if [[ "$VERSION" == "latest" ]]; then
  VERSION="$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" | sed -n 's/.*"tag_name": *"v\{0,1\}\([^"]*\)".*/\1/p' | head -n 1)"
  if [[ -z "$VERSION" ]]; then
    echo "Could not find the latest release of $REPO." >&2
    exit 1
  fi
fi
VERSION="${VERSION#v}"
BASE="https://github.com/$REPO/releases/download/v$VERSION"
ZIP="stereophield-$VERSION-AU.zip"
SUMS="stereophield-$VERSION-SHA256.txt"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "Downloading stereophield $VERSION..."
curl -fsSL -o "$WORK/$ZIP" "$BASE/$ZIP"
curl -fsSL -o "$WORK/$SUMS" "$BASE/$SUMS"

echo "Checking the SHA-256 checksum..."
( cd "$WORK" && grep -E " (\./)?$ZIP\$" "$SUMS" | shasum -a 256 -c - >/dev/null ) || {
  echo "The checksum does not match; nothing was installed." >&2
  exit 1
}

ditto -x -k "$WORK/$ZIP" "$WORK/unzipped"
if ! codesign --verify --deep --strict "$WORK/unzipped/stereophield.component" 2>/dev/null; then
  echo "The plugin's code signature is not valid; nothing was installed." >&2
  exit 1
fi

mkdir -p "$DEST"
rm -rf "$DEST/stereophield.component"
ditto "$WORK/unzipped/stereophield.component" "$DEST/stereophield.component"
xattr -dr com.apple.quarantine "$DEST/stereophield.component" 2>/dev/null || true

# Make hosts rescan their Audio Units.
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo "Installed stereophield $VERSION to $DEST/stereophield.component"
echo "Restart your host (or rescan plug-ins) to load it."
