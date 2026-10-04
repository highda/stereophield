#!/usr/bin/env bash
# Builds the release artefacts: an installer package (Audio Unit and
# Standalone app) and zip files of each, in build/dist.
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

ARTEFACTS="$REPO_ROOT/build/stereophield_artefacts/Release"
PACKAGE_DIR="$REPO_ROOT/build/package"
ROOT_DIR="$PACKAGE_DIR/root"
OUTPUT_DIR="$REPO_ROOT/build/dist"

"$REPO_ROOT/scripts/build.sh" --plugin

rm -rf "$PACKAGE_DIR" "$OUTPUT_DIR"
mkdir -p "$ROOT_DIR/Library/Audio/Plug-Ins/Components" "$ROOT_DIR/Applications" "$OUTPUT_DIR"
cp -R "$ARTEFACTS/AU/stereophield.component" "$ROOT_DIR/Library/Audio/Plug-Ins/Components/"
cp -R "$ARTEFACTS/Standalone/stereophield.app" "$ROOT_DIR/Applications/"

pkgbuild \
  --root "$ROOT_DIR" \
  --identifier "com.highda.stereophield" \
  --version "$VERSION" \
  --install-location "/" \
  "$OUTPUT_DIR/stereophield-${VERSION}-macOS.pkg"

ditto -c -k --keepParent "$ARTEFACTS/AU/stereophield.component" "$OUTPUT_DIR/stereophield-${VERSION}-AU.zip"
ditto -c -k --keepParent "$ARTEFACTS/Standalone/stereophield.app" "$OUTPUT_DIR/stereophield-${VERSION}-Standalone.zip"

( cd "$OUTPUT_DIR" && shasum -a 256 ./* > "stereophield-${VERSION}-SHA256.txt" )
echo "Release artefacts in $OUTPUT_DIR:"
ls -1 "$OUTPUT_DIR"
