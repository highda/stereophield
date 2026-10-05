#!/usr/bin/env bash
# Builds the release artefacts in build/dist: the Audio Unit as a zip file and
# its SHA-256 checksum (read by scripts/install-au.sh).
#
#   ./scripts/package-macos.sh [version]      version defaults to the CMake project version
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_VERSION="$(sed -n 's/^project(stereophield VERSION \([0-9.]*\).*/\1/p' "$REPO_ROOT/CMakeLists.txt")"
VERSION="${1:-$CMAKE_VERSION}"
if [[ "$VERSION" != "$CMAKE_VERSION" ]]; then
  echo "Version $VERSION does not match the CMake project version $CMAKE_VERSION." >&2
  exit 1
fi

COMPONENT="$REPO_ROOT/build/stereophield_artefacts/Release/AU/stereophield.component"
OUTPUT_DIR="$REPO_ROOT/build/dist"

"$REPO_ROOT/scripts/build.sh" --plugin

# The linker signs the component ad hoc; hosts load it without a quarantine flag.
codesign --verify --deep --strict "$COMPONENT"

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"
ditto -c -k --keepParent "$COMPONENT" "$OUTPUT_DIR/stereophield-${VERSION}-AU.zip"

( cd "$OUTPUT_DIR" && shasum -a 256 ./* > "stereophield-${VERSION}-SHA256.txt" )
echo "Release artefacts in $OUTPUT_DIR:"
ls -1 "$OUTPUT_DIR"
