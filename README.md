# stereophield

Mono-to-stereo widening Audio Unit and Standalone app for macOS on Apple Silicon. By default its output sums back to the untouched input in mono. The full specification is in [docs/DESIGN.md](docs/DESIGN.md).

Status: under construction. Progress is tracked by `phase N:` commits; see [docs/DECISIONS.md](docs/DECISIONS.md) and [docs/MEASUREMENTS.md](docs/MEASUREMENTS.md).

## Requirements

- macOS 12 or later on arm64
- Command Line Tools (`xcode-select --install`); full Xcode is not needed
- CMake 3.22 or newer, Ninja recommended

JUCE and Catch2 are fetched by CMake.

## Build and test

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
ctest --test-dir build --output-on-failure
```

The build copies the component to `~/Library/Audio/Plug-Ins/Components/stereophield.component`. Validate it with:

```bash
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aufx Stph Hgda
```
