# Decisions

Dated list of choices not dictated by `DESIGN.md`, and every fallback applied.

## 2026-10-03

- Repository initialised with `git init -b main`; public upstream `highda/stereophield` created with `gh repo create` and set as `origin`. `.gitignore` excludes `build/` and `.DS_Store`.
- First run stopped at the toolchain check (section 3.1): only the Command Line Tools were installed (`xcode-select -p` printed `/Library/Developer/CommandLineTools`, `xcodebuild` unavailable). No code written yet; phase 0 starts on the next run. Other checks passed: arm64, CMake 4.0.2, ninja present, `auval` present, `origin` set.
- Section 3.1 amended at the owner's request: full Xcode is no longer required. The Command Line Tools on this machine provide Apple clang 21, the macOS 26.5 SDK with AudioToolbox, AudioUnit, CoreAudio, CoreAudioKit, CoreMIDI and Accelerate, plus `codesign`, `Rez` and `auval`. The run stops only if no Apple toolchain is present or the phase 0 Audio Unit cannot be built.

### Phase 0

- JUCE pinned to tag `9.0.3` (newest stable on 2026-10-03). Catch2 pinned to `v3.16.0`.
- `DESIGN.md` moved to `docs/DESIGN.md` rather than copied, so there is one copy of the spec.
- `sph_dsp` compiles only project code. It uses the include paths and definitions of `juce_core`, `juce_audio_basics` and `juce_dsp`, and passes those modules to its consumers as interface dependencies, so the JUCE sources compile once per final binary and never twice into one link.
- "Plugin shared code" is an interface library `sph_plugin_code` holding `src/plugin` and `src/ui` plus the JUCE modules. The plugin, `sph_plugin_tests` and `sph_measure` all link it.
- `sph_tests`, `sph_plugin_tests` and `sph_measure` are created with `juce_add_console_app` so they get JUCE's standard definitions, and are written to `build/` directly.
- Tests of the full `AudioProcessor` live in `tests/plugin/` (the layout in section 3.3 lists only `tests/unit/`).
- Catch2 tests are registered per test case with `catch_discover_tests`.

### Phase 1

- `Biquad` is a double-precision transposed direct form II section whose coefficients come from `juce::dsp::IIR::ArrayCoefficients<double>`. These compute the same values as `IIR::Coefficients<float>::makeAllPass` and friends but return a `std::array` instead of a heap-allocated reference-counted object, so coefficients can be recomputed on the audio thread (section 5.1 rule 1).
- `DelayLine` rounds its buffer up to a power of two; `push` then `read(0)` returns the sample just pushed. Lagrange reads use taps `k .. k+3` with the read point in `[1, 2)` where possible; reads are clamped to `[0, maxDelay]`.
- `Stft` synthesis scale is computed as `2 * H / N` (0.5 for `H = N / 4`). JUCE's real inverse transform divides by `N` (the vDSP back end used on macOS too); T1 confirms unity gain at all three rates.
- Test signals: `decayTone` amplitude 0.25; `melody` notes at 0.32 s spacing (300 ms note plus 20 ms gap) starting at 0, peak-normalised to 0.5; `toneClick` is 2 s long and the -18 dBFS level is the sine's peak; `clicks` start at n = 0; fades are raised cosine; `mix` is 3 s long and longer renders tile it; `noise at -30 dB` and `clicks at -12 dB` are gains applied to those signals.
- Measurements live in `tests/common/` and are shared: Catch2 tests assert on them and `sph_measure` writes the same values to `docs/MEASUREMENTS.md`. Levels in dB are RMS ratios to the input over the measured span.

### Phase 2

- All of `src/dsp/` hangs off one `Core` class that runs the graph of section 4.1 from a plain `Params` struct. The plugin reads the 50 APVTS atomics once per block into `Params`; tests can drive `Core` or the full processor.
- Host blocks larger than the prepared size are split into chunks of at most `maxBlockSize`; each chunk is one "block" for sleep decisions. Modules recompute coefficients in sub-blocks of 64 samples.
- Structural changes (choices, `spread_density`, and the source selector) use a `ChangeFader` inside each generator: fade out over 20 ms, apply, reset the module's state, fade in. A pending source is honoured when deciding which buses must be computed.
- Global on/off switches that are not generator choices (`guard`, `comp_mode`, `bass_mono_hz` reaching 20 Hz, the band split turning on or off, `bypass`) crossfade over 20 ms between the two paths instead of switching abruptly. Each path is computed only while its weight is non-zero; at exactly 0 or 1 the output is bit-identical to the unswitched path, so T2 and T27 are unaffected. `listen` switches immediately.
- Band split: "equal and settled" is judged on the target values and the band smoothers; the split's crossfade to the common-gain path takes 20 ms.
- `Spread` Cascade tail is `max(100 ms, 1.5 x the time the slowest section takes to decay 100 dB)`, so narrow low sections (low `spread_f_lo`, high `spread_q`) are not cut off by smart disable. Delay type tail is the delay time.
- Engine switching fades the whole output (dry included) to zero over 20 ms before switching and resetting, then fades back in, so the alignment-delay reset is a short gap rather than a click.
- Values below 1e-30 in magnitude are flushed to zero at the side-bus and output-stage outputs (T20); this changes nothing above -600 dBFS.
- T2 random parameter sets draw every parameter uniformly in its normalised range, then force `mid_blend` 0, `comp_mode` Mono-exact, `listen` Stereo and `bypass` off (the invariant is defined for the processed stereo output). The residual is float rounding of `M +- S`; it grows with the side level relative to the mid.
- Plugin state stores the current program index alongside the APVTS state.
