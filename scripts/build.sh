#!/usr/bin/env bash
# Builds stereophield from a fresh clone: checks the toolchain, installs the
# build tools with Homebrew, configures, builds and installs the Audio Unit.
#
#   ./scripts/build.sh              Release build; installs the AU to ~/Library
#   ./scripts/build.sh --test       also runs every test (about 4 minutes)
#   ./scripts/build.sh --validate   also runs auval on the installed AU
#   ./scripts/build.sh --clean      removes the build directory first
#   ./scripts/build.sh --plugin     builds only the AU and the Standalone app
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
RUN_TESTS=0
VALIDATE=0
CLEAN=0
PLUGIN_ONLY=0

for arg in "$@"; do
  case "$arg" in
    --test) RUN_TESTS=1 ;;
    --validate) VALIDATE=1 ;;
    --clean) CLEAN=1 ;;
    --plugin) PLUGIN_ONLY=1 ;;
    -h|--help) sed -n '2,10p' "$0"; exit 0 ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

step() { printf '\n==> %s\n' "$1"; }

step "Checking the system"
if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "stereophield builds on macOS only." >&2
  exit 1
fi
if [[ "$(uname -m)" != "arm64" ]]; then
  echo "stereophield builds for Apple Silicon (arm64) only." >&2
  exit 1
fi
if ! xcode-select -p >/dev/null 2>&1; then
  echo "The Xcode Command Line Tools are missing. Install them with:" >&2
  echo "  xcode-select --install" >&2
  exit 1
fi
if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew is required. Install it from https://brew.sh, then run this script again." >&2
  exit 1
fi

step "Installing build tools (Homebrew: cmake, ninja)"
brew bundle --file "$REPO_ROOT/Brewfile" --quiet

if [[ $CLEAN -eq 1 ]]; then
  step "Removing $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

step "Configuring (JUCE and Catch2 are fetched on the first run)"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release

step "Building"
JOBS="$(sysctl -n hw.ncpu)"
if [[ $PLUGIN_ONLY -eq 1 ]]; then
  cmake --build "$BUILD_DIR" --target stereophield_AU stereophield_Standalone -j "$JOBS"
else
  cmake --build "$BUILD_DIR" -j "$JOBS"
fi

# The build copies the component to ~/Library/Audio/Plug-Ins/Components.
# Restarting the registrar makes hosts see the new version.
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

if [[ $RUN_TESTS -eq 1 ]]; then
  step "Running the tests (serially, so the CPU tests are not disturbed)"
  ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

if [[ $VALIDATE -eq 1 ]]; then
  step "Validating the Audio Unit"
  auval -v aufx Stph Hgda | tail -n 3
fi

step "Done"
echo "Audio Unit:  ~/Library/Audio/Plug-Ins/Components/stereophield.component"
echo "Standalone:  $BUILD_DIR/stereophield_artefacts/Release/Standalone/stereophield.app"
