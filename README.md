# stereophield

A mono-to-stereo widening Audio Unit and Standalone app for macOS on Apple Silicon, built from classical signal processing only. By default its output sums back to the untouched input in mono: every algorithm adds to the side signal, and the mid is only delayed, never filtered.

The full specification is [docs/DESIGN.md](docs/DESIGN.md). Every choice the specification left open, every deviation and every tuning step is logged in [docs/DECISIONS.md](docs/DECISIONS.md). The measured value of every test is in [docs/MEASUREMENTS.md](docs/MEASUREMENTS.md).

![stereophield, Full engine](docs/ui-full.png)

## What it does

Five generators each synthesise a stereo difference signal from one bus of the input. They run in parallel, and their sum goes through one side bus.

| Generator | Technique |
| --- | --- |
| Spread | All-pass side signal: an integer delay (Lauridsen) or a cascade of second-order all-passes (Orban comb, shaped spread) |
| Delay | Haas delay with an optional low-pass; mono-safe half-depth comb, or the authentic `L = x, R = delayed x` with mid blend |
| Mod | Dimension-style anti-phase chorus, or a two-tap micro-pitch doubler (left up, right down) |
| Velvet | Sparse velvet-noise decorrelator, a different sequence per side |
| Pan map | Adaptive spectral panning: a static complementary split, stable per-partial positions, or whole sources grouped by harmonicity and onset (Full engine only) |

Two analysis engines feed them:

- **Light**: no latency. Every generator reads the input mid, and a time-domain transient detector drives the duck.
- **Full**: one FFT frame of latency (2048 samples up to 50 kHz, 4096 up to 100 kHz, 8192 above). An STFT median-filter split gives tonal, noise and transient buses, an ambience estimate moves decaying energy from tonal to noise, and partial tracking with source grouping drives the Pan map.

The side bus applies bass mono (LR4 high-pass), three width bands, transient ducking, master width and a correlation guard that keeps side energy at or below mid energy. **Smart disable** stops computing any module with zero gain or silent input, without changing the output (T17) or the latency.

`mid_blend` at 0 (the default) is mono-safe: `(L + R) / 2` equals the delayed input exactly, within float rounding (T2 measures -131.8 dB). Raising it adds each generator's mid component back, for the authentic, non-mono-safe sound; the interface then shows "not mono-safe".

The 17 factory presets cover the classic techniques (Haas, Lauridsen, Orban, shaped spread, spectral split, Dimension chorus, micro-pitch, velvet) and four adaptive scenes. They appear as host programs and in the preset menu.

## Requirements

- macOS 12 or later on Apple Silicon (arm64 only)
- Command Line Tools (`xcode-select --install`); full Xcode is not needed
- CMake 3.22 or newer, and Ninja (or Unix Makefiles)

JUCE 9.0.3 and Catch2 v3.16.0 are fetched by CMake on the first configure.

## Build, test and measure

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
ctest --test-dir build --output-on-failure
./build/sph_measure --all --out docs/MEASUREMENTS.md --renders build/renders
```

- `sph_tests` holds the unit tests of `src/dsp/` (building blocks, T1, T7 to T12).
- `sph_plugin_tests` drives the full `AudioProcessor` offline (T2 to T6 and T13 to T28, parameter table, presets, all six sample rates, engine switching).
- `sph_measure` runs the same measurements, rewrites `docs/MEASUREMENTS.md`, regenerates `docs/ui.png` and `docs/ui-full.png`, and renders every factory preset on the `mix` test signal to `build/renders/*.wav` for listening. `sph_measure T13 T14` runs selected tests only.

Timing tests (T18, T25) are only meaningful in a Release build.

## Install and validate

The build copies the Audio Unit to `~/Library/Audio/Plug-Ins/Components/stereophield.component` (ad-hoc signed). Validate it with:

```bash
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aufx Stph Hgda
```

The output must end with `AU VALIDATION SUCCEEDED`. The Standalone app is at `build/stereophield_artefacts/Release/Standalone/stereophield.app`. It asks for microphone access the first time it opens an audio input.

| Item | Value |
| --- | --- |
| Type, subtype, manufacturer | `aufx`, `Stph`, `Hgda` |
| Bundle identifier | `com.highda.stereophield` |
| Channels | mono in or stereo in, stereo out |
| Latency at 48 kHz | Light 0 samples; Full 2048 samples (42.7 ms) |
| CPU, 60 s at 48 kHz, M3 | preset 16 (Full engine) about 2.5 % of real time; preset 1 about 0.16 % |

## Repository layout

```text
src/dsp/       all signal processing (Core runs the whole graph from a Params struct)
src/plugin/    AudioProcessor, parameters, presets
src/ui/        editor, look and feel, displays
tests/unit/    module tests (Catch2)
tests/plugin/  full-processor tests (Catch2)
tests/common/  measurements shared by the tests and sph_measure
tests/signals/ generated test signals
tests/measure/ sph_measure
docs/          specification, decisions, measurements, interface snapshots
```

## Licence

JUCE has its own licence terms for distributed products; see the JUCE repository. The JUCE splash settings are the defaults.
