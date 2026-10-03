# stereophield — design document and build brief

3 October 2026 · highda

## 1. Mission and operating rules

You are a coding agent. Build **stereophield**, a mono-to-stereo widening plugin for macOS on Apple Silicon, from this document alone, and do not stop until every item in section 11.4 (Definition of done) is true.

This document is the complete specification. Where it gives a formula, a constant or a pass criterion, use it exactly. Where it is silent, choose the simplest option consistent with the rest and record the choice in `docs/DECISIONS.md`.

### 1.1 Rules you must follow

1. **Do not ask the user questions.** Nobody is watching the run. The only permitted reason to stop early is listed in section 3.1 (no usable Apple toolchain).
2. **Work in the repository you were given.** It is already initialised with git and has an upstream named `origin` (repository `highda/stereophield`). Do not create a new repository.
3. **Build in the order of section 11.1.** Each phase ends with a gate: a set of tests from section 10 that must pass. Do not start the next phase until the gate passes.
4. **Every DSP module gets tests before it is wired into the plugin.** Measure behaviour with numbers. Never judge audio code by reading it.
5. **Re-run all earlier tests at every gate.** A phase that breaks an earlier test is not finished.
6. **Commit after every passing gate and push to `origin`.** Use the message format `phase N: <summary>`. If the push fails, keep committing locally, note it in `docs/DECISIONS.md`, and continue.
7. **If a test fails, fix the code, not the test.** Section 10.3 marks which criteria are fixed and which constants you may tune.
8. **If a tunable criterion still fails after three different approaches, apply the fallback in section 11.3, log it, and continue.** Never stop the run because of a tunable criterion.
9. **Do not add features that are not in this document.** No reverb, no machine learning, no extra plugin formats.
10. **Obey the real-time rules in section 5.1 in all audio-thread code.**
11. **Keep `docs/MEASUREMENTS.md` current.** It records the measured value for every test in section 10.

### 1.2 Files you must maintain

| File | Content |
| --- | --- |
| `docs/DESIGN.md` | A copy of this document, saved at the start of the run |
| `docs/DECISIONS.md` | Dated list of every choice you made that this document did not dictate, and every fallback applied |
| `docs/MEASUREMENTS.md` | Table of test id, criterion, measured value, pass or fail |
| `README.md` | What the plugin is, how to build, test, install and validate it |

## 2. Product definition

stereophield turns a mono signal into a stereo one using classical signal processing only, and by default its output sums back to the untouched input when played in mono.

### 2.1 Identity

| Item | Value |
| --- | --- |
| Plugin name | stereophield |
| Manufacturer name | highda |
| Manufacturer code | `Hgda` |
| Plugin code | `Stph` |
| Bundle identifier | `com.highda.stereophield` |
| Audio Unit type | `aufx` (effect) |
| Version | 1.0.0 |
| Formats | Audio Unit (AU v2 component) and Standalone application |
| Platform | macOS 12 or later, arm64 only |
| Channel layouts | mono in to stereo out, and stereo in to stereo out |
| Sample rates | 44.1, 48, 88.2, 96, 176.4, 192 kHz |

### 2.2 What it contains

- **Five generators** that each synthesise stereo difference signal: Spread, Delay, Mod, Velvet and Pan map.
- **An analysis stage** with two engines: Light (zero latency) and Full (spectral, one FFT frame of latency).
- **A side bus** with bass mono, three width bands, transient ducking and a correlation guard.
- **Smart disable**, which stops computing any module that has zero gain or silent input.
- **Factory presets** that reproduce the classic techniques (Haas, Lauridsen, Orban, Gerzon-style shaped spread, spectral split, Dimension chorus, micro-pitch doubling, velvet decorrelation) and adaptive scenes that combine them.
- **A full custom user interface** with a goniometer, a correlation meter and a pan map display.

### 2.3 Vocabulary

| Term | Meaning |
| --- | --- |
| Mid, `M` | (left + right) / 2. For a mono input, the input itself |
| Side, `S` | (left - right) / 2 |
| Dry mid | The input mid signal, delayed only by the reported latency, never filtered |
| Mono-safe | (left + right) / 2 at the output equals the dry mid exactly |
| Generator | A module that produces a side signal and an optional mid-difference signal |
| Bus | One of the component signals produced by the analysis stage |
| Engine | Light or Full analysis |
| Asleep | A module that smart disable has stopped processing |

### 2.4 Out of scope

Machine learning, reverb, VST3, AAX, AUv3, Intel Macs, Windows, Linux, notarisation, an installer, a preset browser beyond the factory list and host-saved state, and MIDI.

## 3. Environment, repository and build

The build uses CMake, JUCE and Catch2 on a MacBook Air M3, and nothing else needs to be downloaded.

### 3.1 Check the toolchain first

Run these before writing any code:

```bash
uname -m                 # must print arm64
xcode-select -p          # must print a developer directory (Command Line Tools or Xcode.app)
clang++ --version        # must print an Apple clang version
cmake --version          # must be 3.22 or newer
which auval              # must print a path
git remote -v            # must list origin
```

- If CMake is missing or too old, run `brew install cmake`.
- If `ninja` exists, use the Ninja generator. Otherwise use `Unix Makefiles`.
- Full Xcode is not required. The Command Line Tools provide the compiler, the macOS SDK with all audio frameworks, `codesign` and `auval`, and the build uses CMake with Ninja or Makefiles, never the Xcode generator.
- If `xcode-select -p` or `clang++` fails, or the phase 0 pass-through Audio Unit cannot be built with the installed tools after three attempts, this is the one case where you stop. Print: `Install the Command Line Tools with xcode-select --install (or Xcode from the App Store), then restart the agent.`

### 3.2 Dependencies

| Dependency | How to get it | Version |
| --- | --- | --- |
| JUCE | CMake `FetchContent` from `https://github.com/juce-framework/JUCE` | Newest stable release tag. Pin it with `GIT_TAG` and record it in `docs/DECISIONS.md` |
| Catch2 | CMake `FetchContent` from `https://github.com/catchorg/Catch2` | Newest v3 tag, pinned the same way |

JUCE 9 was released in 2026. If the newest major version causes build errors you cannot resolve in three attempts, pin tag `8.0.12` instead and log it. Use only these JUCE modules: `juce_core`, `juce_events`, `juce_data_structures`, `juce_audio_basics`, `juce_audio_devices`, `juce_audio_formats`, `juce_audio_processors`, `juce_audio_utils`, `juce_audio_plugin_client`, `juce_dsp`, `juce_graphics`, `juce_gui_basics`, `juce_gui_extra`.

### 3.3 Repository layout

```text
CMakeLists.txt
README.md
docs/            DESIGN.md, DECISIONS.md, MEASUREMENTS.md, ui.png
src/dsp/         all signal processing; no GUI code; one class per file
src/plugin/      PluginProcessor, Parameters, Presets, SleepController wiring
src/ui/          PluginEditor and all components
tests/unit/      Catch2 tests, one file per module
tests/measure/   sph_measure command-line tool
tests/signals/   TestSignals.h (generated signals, no audio files in the repo)
```

### 3.4 CMake targets

| Target | Kind | Links | Purpose |
| --- | --- | --- | --- |
| `sph_dsp` | static library | `juce_core`, `juce_audio_basics`, `juce_dsp` | Everything in `src/dsp/` |
| `stereophield` | `juce_add_plugin` | `sph_dsp` plus the plugin and GUI modules | The AU and Standalone |
| `sph_tests` | executable | `sph_dsp`, `Catch2::Catch2WithMain` | Unit tests, registered with CTest |
| `sph_plugin_tests` | executable | plugin shared code, Catch2 | Tests that need the full `AudioProcessor` |
| `sph_measure` | `juce_add_console_app` | plugin shared code | Renders audio and writes measurements |

The `juce_add_plugin` call must set:

```cmake
juce_add_plugin(stereophield
    COMPANY_NAME "highda"
    BUNDLE_ID "com.highda.stereophield"
    PLUGIN_MANUFACTURER_CODE Hgda
    PLUGIN_CODE Stph
    FORMATS AU Standalone
    PRODUCT_NAME "stereophield"
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)
```

Set C++20, `CMAKE_OSX_ARCHITECTURES=arm64` and `CMAKE_OSX_DEPLOYMENT_TARGET=12.0`. Compile with `-Wall -Wextra` and fix all warnings in your own code.

### 3.5 Commands

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
ctest --test-dir build --output-on-failure
./build/sph_measure --all --out docs/MEASUREMENTS.md --renders build/renders
```

Validate the Audio Unit after each plugin build:

```bash
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aufx Stph Hgda
```

`auval` must end with `AU VALIDATION SUCCEEDED`. The component is installed to `~/Library/Audio/Plug-Ins/Components/stereophield.component` by `COPY_PLUGIN_AFTER_BUILD`. Ad-hoc code signing, which JUCE applies by default, is enough.

### 3.6 Licence note for the owner

JUCE has its own licence terms for distributed products. This is the owner's decision and does not affect the build. Do not add licence keys or change JUCE splash settings beyond the defaults.

## 4. Signal architecture

The dry mid signal is never processed, and every algorithm contributes only to a side bus; this single rule is what makes the output mono-safe however the generators are combined.

### 4.1 Signal flow

```text
 input (mono or stereo)
   |
   v
 [Input split]  M = (L+R)/2      S_in = (L-R)/2      (mono input: M = x, S_in = 0)
   |
   +--> [Alignment delay, Lat samples] --> M_d , S_in_d          (never filtered)
   |
   +--> [Analysis]  Light engine: transient envelope e(n); every bus = M
   |                Full engine:  STFT -> buses FULL, TONAL, NOISE (all delayed by Lat)
   |                              transient envelope e(n), tonal spectrum for Pan map
   |
   |          each generator reads ONE bus, chosen by its "source" parameter
   |
   +--> [Spread ] --> s1, m1 --+
   +--> [Delay  ] --> s2, m2 --+
   +--> [Mod    ] --> s3, m3 --+--> S_bus = sum(amount_g * s_g)
   +--> [Velvet ] --> s4, m4 --+    D_bus = sum(amount_g * m_g)
   +--> [Pan map] --> s5, 0  --+
                                |
                                v
 [Side bus]  S_bus: high-pass -> 3 width bands -> transient duck -> guard -> S_syn
             D_bus: the same high-pass only                              -> D_syn
                                |
                                v
 [Output]    M_out = M_d + mid_blend * D_syn
             S_out = width * S_syn + S_in_d
             L = M_out + S_out        R = M_out - S_out
             -> loudness compensation -> output gain -> listen mode -> meters
```

### 4.2 Generator contract

Every generator receives one mono bus signal `u(n)` and returns two mono signals:

- `s(n)`, the **side** contribution.
- `m(n)`, the **mid difference**: what would have to be added to the mid channel to obtain the generator's authentic, non-mono-safe sound. Generators that are purely side-based return `m = 0`.

Section 6.3 gives `s` and `m` for each generator.

### 4.3 The mono-safe invariant

With `mid_blend = 0` and compensation mode `Mono-exact`, the output satisfies:

```text
(L + R) / 2  =  out_gain * M_d        exactly, for every parameter setting
```

This holds because `S_out` cancels in the sum and `M_d` is only delayed. Test T2 enforces it. Nothing may ever be inserted in the `M_d` path except the alignment delay.

### 4.4 Engines and latency

| Engine | What runs | Reported latency `Lat` |
| --- | --- | --- |
| Light | Time-domain transient detector only. All buses equal `M`. Pan map is unavailable | 0 samples |
| Full | STFT analysis, component split, ambience split, partial tracking, grouping | `N` samples, where `N` is the FFT size from section 6.2 |

- The engine is an explicit user parameter. It is the **only** thing that changes the reported latency.
- Smart disable never changes latency. A sleeping module skips computation while the alignment delay keeps running.
- Delays inside generators (Haas time, chorus base delay, pitch-shifter window) are part of the effect and are **not** reported or compensated.
- Verify `Lat` by measurement in test T3. If the measured STFT latency differs from `N`, report the measured value.

**Switching engine.** When `engine` changes: fade the synthesised side to zero over 20 ms, switch, `reset()` every module and the alignment delay, then fade back in. Call `setLatencySamples()` from the message thread (use `juce::AsyncUpdater`), never from the audio thread. In `isBusesLayoutSupported`, accept a mono or stereo input and only a stereo output.

### 4.5 Why generators run in parallel

Generators are summed, never chained. Chaining two decorrelators multiplies their colouration in each channel. Parallel contributions simply add on the side bus, and each can sleep independently.

## 5. DSP conventions

All modules share one interface, one set of real-time rules and one set of numeric conventions, so that they can be tested alone and combined without surprises.

### 5.1 Real-time rules for audio-thread code

1. No memory allocation or deallocation. Size every buffer in `prepare()`.
2. No locks, no file or console output, no system calls.
3. No exceptions thrown or caught.
4. Put `juce::ScopedNoDenormals` at the top of `processBlock`.
5. Parameters are read from atomics once per block.
6. Data for the user interface leaves the audio thread only through atomics or a single-producer single-consumer lock-free FIFO.

Test T19 counts allocations during processing and must find zero.

### 5.2 Module interface

Every class in `src/dsp/` follows this shape. Argument lists differ per module; the method names do not.

```cpp
struct ProcessSpec { double sampleRate; int maxBlockSize; };

class Module {
public:
    void prepare (const ProcessSpec&);   // allocate, compute coefficients, then reset()
    void reset();                        // clear all state to silence; no allocation
    void process (/* input and output pointers, numSamples */);
    int  tailSamples() const;            // samples of output after input stops
    void advanceWhileAsleep (int numSamples);  // only for modules with LFOs or phasors
};
```

### 5.3 Numeric conventions

| Topic | Convention |
| --- | --- |
| Sample type | `float` for audio, `double` for coefficient design and phase accumulators |
| Internal block size | The host block is split into sub-blocks of at most 64 samples. Parameters are updated once per sub-block |
| Gain smoothing | Linear ramp over 20 ms using `juce::SmoothedValue`, applied per sample |
| Delay-time smoothing | One-pole low-pass with a 50 ms time constant, applied per sample |
| Frequency smoothing | Recompute filter coefficients once per sub-block from a 20 ms smoothed value |
| Choice parameters | On change: fade the affected generator's output to zero over 20 ms, switch, `reset()`, fade back in |
| Percent parameters | 0 to 100 % maps to 0.0 to 1.0. `width` and band widths map 0 to 200 % to 0.0 to 2.0 |
| Decibels | `dB = 20 * log10(amplitude)`. Full scale is amplitude 1.0 |
| One-pole coefficient | `a = exp(-1 / (tau_seconds * rate))`, then `y = a * y + (1 - a) * x`, where `rate` is the sample rate or the frame rate |

### 5.4 Shared building blocks

Implement these first, each with its own tests:

- **DelayLine**: circular buffer with integer read, linear-interpolated read and third-order Lagrange read. Maximum length set in `prepare()`.
- **OnePole**: the smoother in section 5.3.
- **Biquad** helpers: use `juce::dsp::IIR` for high-pass, all-pass and low-pass sections.
- **Stft**: forward and inverse transform with overlap-add, defined in section 6.2.
- **Rng**: a small deterministic generator (for example xorshift32) with an explicit seed. Never use `rand()` or a time-based seed.
- **PeakScanner**: returns the absolute peak of a block using `juce::FloatVectorOperations::findMinAndMax`.

## 6. Module specifications

Each module below is specified by its inputs, outputs, exact formulas and constants. Constants marked *tunable* may be adjusted to pass a tunable test; all others are fixed.

### 6.1 Input split and alignment

- Mono input `x`: `M = x`, `S_in = 0`.
- Stereo input: `M = (L + R) / 2`, `S_in = (L - R) / 2`.
- `M_d` and `S_in_d` are `M` and `S_in` delayed by exactly `Lat` samples with an integer delay line.
- When the bypass parameter is on, output `L = M_d + S_in_d`, `R = M_d - S_in_d`, so bypass stays latency-aligned.

### 6.2 Analysis

#### 6.2.1 Transient detector (both engines)

Runs on the undelayed `M`, sample by sample:

```text
x      = abs(M[n])
fast   = follower(x, attack 0.1 ms, release 30 ms)
slow   = follower(x, attack 15 ms,  release 30 ms)
r      = fast / (slow + 1e-6)
e_raw  = (fast < 1e-4) ? 0 : clamp((r - 1.5) / 1.5, 0, 1)
e[n]   = max(e_raw, e[n-1] * exp(-1 / (0.040 * fs)))
```

`follower` uses the one-pole of section 5.3 with the attack coefficient when the input is above the state and the release coefficient otherwise. The thresholds 1.5 and the 40 ms hold are *tunable*.

- Light engine: use `e[n]` directly.
- Full engine: delay `e[n]` by `Lat - round(0.002 * fs)` samples, so ducking begins 2 ms before the transient arrives on the delayed buses.

#### 6.2.2 STFT (Full engine)

| Sample rate | FFT size `N` | Hop `H` | Bins `K` |
| --- | --- | --- | --- |
| up to 50 kHz | 2048 | 512 | 1025 |
| up to 100 kHz | 4096 | 1024 | 2049 |
| above 100 kHz | 8192 | 2048 | 4097 |

- Window, used for both analysis and synthesis: `w[n] = sqrt(0.5 * (1 - cos(2 * pi * n / N)))` for `n = 0 .. N-1`.
- Analysis: multiply the latest `N` input samples by `w`, transform with `juce::dsp::FFT`.
- Synthesis: inverse transform, multiply by `w`, overlap-add at hop `H`, multiply by 0.5.
- Adjust the final scale factor so that an unmodified spectrum reconstructs the input at unity gain (test T1). JUCE's inverse transform scaling must be checked by this test, not assumed.
- One forward transform per hop. Up to three inverse transforms per hop: TONAL bus, NOISE bus and the Pan map side signal. Skip any inverse transform whose output nobody consumes.
- The FULL bus is simply `M` delayed by `Lat`. The `Tonal+Noise` source is the sum of the two bus signals.
- A fourth inverse transform for the transient component exists only in test builds, for test T10.

Below, `t` is the frame index, `k` the bin index, `X(t,k)` the complex spectrum, and `A(t,k) = abs(X(t,k)) * 2 / sum(w)` the magnitude in input-amplitude units.

#### 6.2.3 Stage A: component split

Median-filter separation into tonal, transient and noise parts, using only past frames so that no extra latency is added.

```text
P(t,k)  = median of A(t, k-8 .. k+8)        17 bins, clamp indices at the edges
H(t,k)  = median of A(t-8 .. t, k)          9 frames, current frame included
r       = H / (H + P + 1e-12)
mt_raw  = clamp((r - 0.45) / 0.30, 0, 1)    tonal
mx_raw  = clamp((0.55 - r) / 0.30, 0, 1)    transient
mt      = 0.5 * mt_prev + 0.5 * mt_raw
mx      = max(mx_raw, 0.5 * mx_prev)
mt      = min(mt, 1 - mx)
mn      = 1 - mt - mx                       noise
```

The three masks always sum to exactly 1. Use `std::nth_element` on fixed-size stack arrays for the medians. The window lengths (17, 9) and breakpoints (0.45, 0.55, 0.30) are *tunable*.

#### 6.2.4 Stage B: ambience split

A heuristic based on late-reverberation spectral estimation: energy that is decaying is treated as ambience and moved from the tonal bus to the noise bus.

```text
Pw      = A(t,k)^2
Ps(t,k) = 0.7 * Ps(t-1,k) + 0.3 * Pw
D       = max(1, round(0.050 * fs / H))                 frames in 50 ms
Pold    = Ps(t-D, k)
R       = exp(-0.6908 / room_decay_s) * Pold            predicted late energy
d       = clamp(1 - Pw / (Pold + 1e-12), 0, 1)          1 when energy is falling
ma      = ambience * min(1, sqrt(R / (Pw + 1e-12))) * d
mt2     = mt * (1 - ma)
mn2     = mn + mt * ma
```

`ambience` and `room_decay_s` are user parameters. The masks `mt2`, `mx`, `mn2` still sum to 1. The smoothing constants 0.7 and 0.3 are *tunable*.

Bus spectra: `TONAL = mt2 * X`, `NOISE = mn2 * X`, transient `= mx * X`.

#### 6.2.5 Stage C: partial tracking

Runs only when Pan map is awake and `pan_mode` is Tracks or Groups.

1. **Peak picking** on `At = mt2 * A`. Bin `k` is a peak if `At(k) > At(k-1)`, `At(k) >= At(k+1)`, `At(k) > 10^(-70/20)`, `At(k) > 10^(-50/20) * max(At)`, and its frequency lies between 40 Hz and 12 kHz. Keep the 60 largest.
2. **Frequency refinement** by parabolic interpolation on log magnitude: with `a, b, c = ln(At(k-1)), ln(At(k)), ln(At(k+1))`, `delta = 0.5 * (a - c) / (a - 2b + c)` and `f = (k + delta) * fs / N`.
3. **Matching.** Visit live tracks in order of decreasing amplitude. Each takes the nearest unclaimed peak with `abs(f_peak / f_track - 1) <= 0.03`. On a match: `freq = 0.5 * freq + 0.5 * f_peak`, `amp = peak amplitude`, `missed = 0`, `age += 1`.
4. **Death.** An unmatched track gets `missed += 1`. When `missed > 3` the track is freed.
5. **Birth.** Each unclaimed peak starts a new track if one of the 96 fixed slots is free: `birthFreq = freq = f`, `birthFrame = t`, `age = 0`, `group = -1`.

Track record: `active, freq, birthFreq, amp, birthFrame, age, missed, group, pan`.

#### 6.2.6 Stage D: source grouping

Runs only when `pan_mode` is Groups. It groups partials that share a harmonic series and an onset time, and gives each group one pan position. At most `pan_max_groups` groups exist.

Group record: `active, f0, onsetFrame, amp, peakAmp, diedFrame, pan`. Each frame, `amp` is the sum of the group's track amplitudes and `peakAmp` is its running maximum. A group with no live tracks is freed and its `f0`, `pan` and `diedFrame` are kept in a small history list.

For each track with `group == -1` and `age >= 2`, apply the first rule that matches:

1. **Harmonic with common onset.** Among active groups, find those where `h = round(freq / f0)` satisfies `1 <= h <= 20`, `abs(freq / (h * f0) - 1) <= 0.03` and `abs(birthFrame - onsetFrame) <= 4`. Join the one with the smallest harmonic error.
2. **New fundamental.** Among active groups, find those where `q = round(f0 / freq)` satisfies `2 <= q <= 4`, `abs(f0 / (q * freq) - 1) <= 0.03` and `abs(birthFrame - onsetFrame) <= 4`. Join it and set the group's `f0 = freq`.
3. **Late weak partial.** As rule 1 without the onset condition, but only if the track's amplitude is at most 0.3 times the strongest track in that group.
4. **New group.** If a group slot is free, create a group with `f0 = freq`, `onsetFrame = birthFrame`, `pan = assignPan(f0)`.
5. **No slot.** Leave the track ungrouped with `pan = 0`.

`assignPan(f0)`:

1. If `f0 < pan_bass_center_hz`, return 0.
2. **Voice continuity.** Collect groups that died within the last 0.4 s, and active groups whose `amp < 0.25 * peakAmp`. If any has a pitch within 7 semitones of `f0`, return the pan of the nearest in pitch. This keeps a melody in one place.
3. Otherwise take the slot list `[-0.5, +0.5, -1.0, +1.0, -0.25, +0.25, -0.75, +0.75]` and return the slot whose minimum distance to the pans of all active groups is largest. On a tie, take the earliest in the list.

The tolerances 0.03, the onset window 4, the factors 0.3 and 0.25, and the 0.4 s memory are *tunable*.

### 6.3 Generators

Every generator has an `amount` parameter and, except Pan map, a `source` parameter with the options Full, Tonal, Noise and Tonal+Noise. In the Light engine every source resolves to `M`. `u` is the selected bus signal.

#### 6.3.1 Spread (all-pass side engine)

`s = A(u)` and `m = 0`, where `A` is an all-pass filter. Because `A` has unit magnitude, the left and right magnitudes are complementary combs, the summed power is flat, and the mono sum is exact.

| `spread_type` | `A` | Controls |
| --- | --- | --- |
| Delay | A pure integer delay of `spread_time_ms` | time |
| Cascade | A series of `spread_density` second-order all-pass sections | density, low and high frequency, skew, Q |

Cascade section centre frequencies, for `i = 0 .. K-1` with `K = spread_density`:

```text
x_i   = (i + 0.5) / K
gamma = 2 ^ spread_skew
f_i   = spread_f_lo * (spread_f_hi / spread_f_lo) ^ (x_i ^ gamma)
f_i   = min(f_i, 0.45 * fs)
```

Each section uses `juce::dsp::IIR::Coefficients<float>::makeAllPass(fs, f_i, spread_q)`. Tail: the delay length for Delay, 100 ms for Cascade.

The Delay type is the Lauridsen method. The Cascade type with default settings is the Orban-style comb. The Cascade type with user-shaped density, range and skew is the shaped spread; frequency-dependent depth comes from the side bus width bands.

#### 6.3.2 Delay (Haas)

```text
d = lowpass(delay(u, delay_time_ms), delay_lp_hz)     one-pole low-pass; bypass it at 20000 Hz
q = (delay_side == Right) ? +1 : -1
s = q * (u - d) / 2
m = (d - u) / 2
```

Use the linear-interpolated delay read. With `mid_blend = 100 %` and `delay_side = Right` this gives exactly `L = u`, `R = d`, the authentic Haas effect. With `mid_blend = 0` it is a mono-safe half-depth comb. Tail: the delay length plus 5 ms.

#### 6.3.3 Mod (chorus and micro-pitch)

Both types use two Lagrange-interpolated delay lines fed with `u`.

**Chorus** (`mod_type = Chorus`), a Dimension-style anti-phase chorus:

```text
phase += mod_rate_hz / fs                       wrap to [0, 1)
tri    = 4 * abs(phase - 0.5) - 1               triangle in [-1, 1]
l      = onepole(tri, tau = 0.05 / mod_rate_hz) rounds the corners
dL     = mod_base_ms + mod_depth_ms * l
dR     = mod_base_ms - mod_depth_ms * l         clamp both to >= 0.5 ms
cL     = read(u, dL)        cR = read(u, dR)
s      = (cL - cR) / 2
m      = 0.5 * ((cL + cR) / 2 - u)
```

**Micro-pitch** (`mod_type = Micro-pitch`), a two-tap crossfading delay-line pitch shifter per side. Left is shifted up by `mod_cents`, right down by the same amount.

```text
W       = 0.040 * fs                            window, 40 ms in samples
ratio   = 2 ^ (cents / 1200)                    cents is +mod_cents (left) or -mod_cents (right)
phi    += abs(ratio - 1) / W                    wrap to [0, 1)
phi2    = frac(phi + 0.5)
up-shift:    d1 = (1 - phi) * W     d2 = (1 - phi2) * W
down-shift:  d1 = phi * W           d2 = phi2 * W
g1 = sin(pi * phi)^2                g2 = sin(pi * phi2)^2        g1 + g2 = 1
pre     = mod_predelay_ms (left), mod_predelay_ms + 8 ms (right)
out     = g1 * read(u, d1 + pre) + g2 * read(u, d2 + pre)
```

With `pL` and `pR` the two shifted outputs: `s = (pL - pR) / 2`, `m = 0.5 * ((pL + pR) / 2 - u)`. If `mod_cents = 0`, hold `phi` still.

Tail for both types: 80 ms. Both keep their phase running while asleep through `advanceWhileAsleep`.

#### 6.3.4 Velvet (sparse decorrelator)

Each side is convolved with its own velvet-noise sequence: a few dozen signed impulses with an exponentially decaying envelope.

```text
Len  = round(velvet_size_ms * 0.001 * fs)                 sequence length in samples
Mimp = max(8, round(velvet_density * velvet_size_ms * 0.001))   number of impulses
Td   = Len / Mimp                                         grid spacing
for j = 0 .. Mimp-1:
    pos_j  = round(j * Td + rnd1() * (Td - 1))            rnd1 uniform in [0, 1)
    sign_j = (rnd2() < 0.5) ? -1 : +1
    g_j    = sign_j * exp(-6.908 * j / Mimp)              reaches -60 dB at the end
normalise so that sum(g_j^2) = 1
v[n] = sum over j of g_j * u[n - pos_j]
```

- Left uses seed `1000 + 2 * velvet_variation`, right uses seed `1001 + 2 * velvet_variation`.
- Rebuild the sequences on the message thread when size, density or variation changes, then swap them in with the 20 ms fade of section 5.3. Pre-allocate for the maximum impulse count (240).
- `s = (vL - vR) / 2`, `m = (vL + vR) / 2 - u`.
- Tail: `Len` samples.

#### 6.3.5 Pan map (adaptive spectral panning)

Available only in the Full engine. It builds a pan value `p(t,k)` in `[-1, +1]` for every bin and outputs the side signal `s = ISTFT(S)`, `m = 0`. A bin with `p = +1` appears only in the left channel, `p = -1` only in the right.

Static curve, used by two of the modes:

```text
c(f) = (f < pan_bass_center_hz) ? 0 : sin(2 * pi * pan_density * log2(f / 100))
```

| `pan_mode` | Target pan `p_target(t,k)` | Side spectrum `S(t,k)` |
| --- | --- | --- |
| Static | `c(f_k)` for every bin | `pan_depth * p * (mt2 + mn2) * X` |
| Tracks | For bins owned by a track: `c(track.birthFreq)`. Others: 0 | `pan_depth * p * mt2 * X` |
| Groups | For bins owned by a track: the pan of the track's group. Others: 0 | `pan_depth * p * mt2 * X` |

- **Bin ownership.** A track with `age >= 1` owns the bins that are nearer to its peak bin than to any other track's peak, limited to `+-6 * N / 2048` bins.
- **Smoothing in time.** `p(t,k) = a * p(t-1,k) + (1 - a) * p_target(t,k)` with `a = exp(-H / (0.030 * fs))`.
- **Smoothing in frequency.** Before use, filter `p` across `k` with the kernel `[0.25, 0.5, 0.25]`.
- **Latency.** The inverse transform already has latency `Lat`, so `s` is aligned with `M_d`.
- **Display.** Publish `p` weighted by magnitude, resampled to 128 log-spaced points from 40 Hz to 16 kHz, for the user interface.
- Tail: `N` samples.

Static mode is the classic complementary spectral split. Tracks mode keeps each partial at a stable position. Groups mode places whole sources.

### 6.4 Side bus

The side bus processes `S_bus` in four steps, in this order. `D_bus` receives step 1 only.

**Step 1: bass mono (high-pass).** A fourth-order Linkwitz-Riley high-pass at `bass_mono_hz`, built as two identical second-order Butterworth high-pass sections in series. When `bass_mono_hz` is at its minimum of 20 Hz, bypass the filter completely. No filter is ever needed on the dry path, because only the side signal carries width.

**Step 2: three width bands.** Use `juce::dsp::LinkwitzRileyFilter` with crossover frequencies `f1 = band_xover_lo` and `f2 = band_xover_hi`:

```text
low   = LP_f1(x)          rest = HP_f1(x)
mid   = LP_f2(rest)       high = HP_f2(rest)
low2  = AP_f2(low)        all-pass at f2 keeps the low band phase-aligned
y     = band_low * low2 + band_mid * mid + band_high * high
```

The split shifts phase. To keep `D_bus` aligned with `S_bus`: when the three band gains are equal and settled, bypass the split on both buses and multiply `S_bus` by the common gain. Otherwise pass `D_bus` through an identical split and recombine it with all gains at 1.

**Step 3: transient duck.** `y = y * (1 - transient_duck * e[n])`, with `e[n]` from section 6.2.1.

**Step 4: width and correlation guard.** First `S_w = width * y`. If `guard` is On:

```text
pm     = onepole(M_out^2, 200 ms)
ps     = onepole(S_w^2,   200 ms)
target = (ps > pm) ? sqrt(pm / (ps + 1e-20)) : 1
g      = follower(target, 5 ms when falling, 100 ms when rising)
S_syn  = g * S_w
```

If `guard` is Off, `S_syn = S_w`. The guard keeps synthesised side energy at or below mid energy, which keeps the left-right correlation at or above zero. It never touches `S_in_d`.

### 6.5 Output stage

```text
M_out = M_d + mid_blend * D_syn
S_out = S_syn + S_in_d
L     = M_out + S_out
R     = M_out - S_out
```

**Loudness compensation** (`comp_mode`):

- Mono-exact: `c = 1`.
- Constant loudness: `pm2 = onepole(M_out^2, 300 ms)`, `ps2 = onepole(S_out^2, 300 ms)`, `c = sqrt(pm2 / (pm2 + ps2))`, and `c = 1` when `pm2 + ps2 < 1e-12`. Multiply both `L` and `R` by `c`. The mono sum keeps its spectrum but loses level.

**Output gain.** Multiply both channels by `10^(out_gain_db / 20)`.

**Listen mode** (`listen`): Stereo outputs `L, R`. Mono outputs `(L + R) / 2` on both channels. Side outputs `(L - R) / 2` on both channels.

**Meters**, computed after listen mode and sent to the user interface:

- Peak level of each channel per block.
- Correlation: `sum(L * R) / sqrt(sum(L^2) * sum(R^2))`, each sum a 100 ms one-pole. Report 0 when the denominator is below `1e-12`.
- Goniometer points: `(L, R)` pairs pushed to a lock-free FIFO, decimated so that at most 2048 pairs arrive per 33 ms.

## 7. Smart disable

A module that cannot affect the output is not computed: it sleeps when its gain is zero or when its input has been silent for longer than its own tail.

### 7.1 Sleep conditions

Evaluate once per host block, for each generator, using one `SleepController` object per module.

| Trigger | Condition to fall asleep | What happens |
| --- | --- | --- |
| Zero gain | The smoothed `amount` is exactly 0 and its ramp has finished. For all generators together: `width` is 0 and settled | Skip `process()`, then call `reset()` once |
| Silent input | The input peak has been below `1e-7` (-140 dBFS) for at least `tailSamples()` consecutive samples | Skip `process()`, then call `reset()` once |

A sleeping module contributes exact zeros to `S_bus` and `D_bus`.

For the silent-input trigger, Pan map and the Full analysis wait `N` samples plus 0.5 s instead of their audio tail. This outlasts the 0.4 s voice-continuity memory, so their state after the wait equals a fresh `reset()`.

### 7.2 Waking

- Wake at the start of the first block where neither condition holds. The peak scan of that block decides, so no sample is missed.
- State is already clear, so no stale audio can burst out.
- After a zero-gain sleep, the existing 20 ms gain ramp fades the module in.
- Waking allocates nothing.

### 7.3 Modules with phase

Mod keeps its chorus phase and pitch-shifter phasors running while asleep. `advanceWhileAsleep(n)` adds `n * increment` to each phase and wraps it. This makes a render identical no matter when the module was woken. The smoothed LFO value must be kept current as well, as test T17 explains.

### 7.4 Cascading

| Stage | Sleeps when |
| --- | --- |
| Tonal inverse transform | No awake generator uses the Tonal or Tonal+Noise source |
| Noise inverse transform | No awake generator uses the Noise or Tonal+Noise source |
| Stages C and D | Pan map is asleep, or `pan_mode` is Static |
| Whole Full analysis (forward transform and stages A and B) | Pan map is asleep and no awake generator uses a source other than Full, or the input has been silent for `N` samples |
| Side bus | Every generator is asleep and the side bus input has been silent for 100 ms |
| Transient detector | `transient_duck` is 0, or the side bus is asleep |

When everything is asleep the plugin only runs the alignment delay and the output matrix.

### 7.5 Things that must not change

1. **Latency.** Sleeping never changes the reported latency. The alignment delay always runs.
2. **STFT framing.** When the Full analysis sleeps, keep feeding input into its FIFO and keep the hop counter running, so that frames stay on the same grid after waking. Only the transforms and stages are skipped.
3. **Host tail.** `getTailLengthSeconds()` returns the longest tail of any awake module plus `Lat / fs`. This lets the host stop calling the plugin on silent tracks.

### 7.6 Debug switch

Provide a non-parameter flag `forceAwake` on the processor, settable only from tests. When true, no module ever sleeps. Tests T17 and T18 compare runs with and without it.

## 8. Parameters and factory presets

The plugin has 50 parameters, all held in one `juce::AudioProcessorValueTreeState`, and 17 factory presets that cover every classic technique and the adaptive scenes.

### 8.1 Rules

- Parameter identifiers are exactly the strings below, with version hint 1.
- Every parameter is automatable except `engine`, which is created with automation disabled because it changes latency.
- Plugin state is the APVTS state serialised to XML in `getStateInformation`.
- Frequency parameters use a skew so that the geometric mean of the range sits at the knob centre.

### 8.2 Global parameters

| ID | Type and range | Default | Meaning |
| --- | --- | --- | --- |
| `engine` | choice: Light, Full | Light | Analysis engine; sets latency |
| `width` | 0 to 200 % | 100 | Master gain of the synthesised side |
| `mid_blend` | 0 to 100 % | 0 | 0 is mono-safe; 100 gives each generator's authentic sound |
| `bass_mono_hz` | 20 to 500 Hz | 150 | Side high-pass; 20 means off |
| `band_xover_lo` | 100 to 1000 Hz | 400 | Low to mid crossover |
| `band_xover_hi` | 1000 to 10000 Hz | 4000 | Mid to high crossover |
| `band_low` | 0 to 200 % | 100 | Width of the low band |
| `band_mid` | 0 to 200 % | 100 | Width of the mid band |
| `band_high` | 0 to 200 % | 100 | Width of the high band |
| `transient_duck` | 0 to 100 % | 50 | Side reduction on transients |
| `guard` | choice: Off, On | On | Correlation guard |
| `comp_mode` | choice: Mono-exact, Constant loudness | Mono-exact | Loudness compensation |
| `out_gain_db` | -24 to +12 dB | 0 | Output gain |
| `listen` | choice: Stereo, Mono, Side | Stereo | Audition mode |
| `bypass` | bool | off | Latency-aligned bypass; also returned by `getBypassParameter()` |
| `ambience` | 0 to 100 % | 50 | Stage B amount (Full engine) |
| `room_decay_s` | 0.3 to 3.0 s | 1.0 | Stage B decay time (Full engine) |

### 8.3 Generator parameters

| ID | Type and range | Default |
| --- | --- | --- |
| `spread_amount` | 0 to 100 % | 60 |
| `spread_source` | choice: Full, Tonal, Noise, Tonal+Noise | Full |
| `spread_type` | choice: Delay, Cascade | Cascade |
| `spread_time_ms` | 1 to 30 ms | 10 |
| `spread_density` | integer 2 to 24 | 8 |
| `spread_f_lo` | 40 to 1000 Hz | 100 |
| `spread_f_hi` | 2000 to 18000 Hz | 10000 |
| `spread_skew` | -1 to +1 | 0 |
| `spread_q` | 0.3 to 4.0 | 0.7 |
| `delay_amount` | 0 to 100 % | 0 |
| `delay_source` | choice, as above | Full |
| `delay_time_ms` | 0.1 to 40 ms | 15 |
| `delay_lp_hz` | 1000 to 20000 Hz | 20000 |
| `delay_side` | choice: Right, Left | Right |
| `mod_amount` | 0 to 100 % | 0 |
| `mod_source` | choice, as above | Full |
| `mod_type` | choice: Chorus, Micro-pitch | Chorus |
| `mod_rate_hz` | 0.05 to 5 Hz | 0.4 |
| `mod_depth_ms` | 0 to 5 ms | 1.5 |
| `mod_base_ms` | 3 to 20 ms | 8 |
| `mod_cents` | 0 to 25 cents | 9 |
| `mod_predelay_ms` | 0 to 30 ms | 12 |
| `velvet_amount` | 0 to 100 % | 0 |
| `velvet_source` | choice, as above | Full |
| `velvet_size_ms` | 10 to 80 ms | 30 |
| `velvet_density` | 500 to 3000 impulses per second | 1000 |
| `velvet_variation` | integer 0 to 15 | 0 |
| `pan_amount` | 0 to 100 % | 0 |
| `pan_mode` | choice: Static, Tracks, Groups | Groups |
| `pan_depth` | 0 to 100 % | 70 |
| `pan_density` | 0.25 to 4 cycles per octave | 1.0 |
| `pan_bass_center_hz` | 60 to 300 Hz | 120 |
| `pan_max_groups` | integer 2 to 8 | 6 |

### 8.4 Factory presets

Each preset starts from the defaults above and changes only the listed values. Expose them through the `AudioProcessor` program interface and through the preset menu in the user interface.

| # | Name | Changes from default |
| --- | --- | --- |
| 1 | Default: Orban comb | none |
| 2 | Classic: Haas | `spread_amount` 0, `delay_amount` 100, `mid_blend` 100, `bass_mono_hz` 20, `transient_duck` 0, `guard` Off |
| 3 | Classic: Haas, mono-safe | `spread_amount` 0, `delay_amount` 100 |
| 4 | Classic: Lauridsen | `spread_type` Delay, `spread_amount` 100, `spread_time_ms` 10 |
| 5 | Classic: Orban, full depth | `spread_amount` 100 |
| 6 | Classic: Shaped spread | `spread_amount` 80, `spread_density` 14, `spread_f_lo` 200, `spread_f_hi` 12000, `spread_skew` 0.5, `spread_q` 1.0, `band_low` 50, `band_high` 130 |
| 7 | Classic: Spectral split | `engine` Full, `spread_amount` 0, `pan_amount` 100, `pan_mode` Static, `pan_depth` 100, `pan_density` 0.5, `transient_duck` 0 |
| 8 | Dimension chorus | `spread_amount` 0, `mod_amount` 100, `mid_blend` 100 |
| 9 | Dimension chorus, mono-safe | `spread_amount` 0, `mod_amount` 100 |
| 10 | Micro-pitch doubler | `spread_amount` 0, `mod_amount` 100, `mod_type` Micro-pitch, `mid_blend` 100 |
| 11 | Micro-pitch, mono-safe | `spread_amount` 0, `mod_amount` 100, `mod_type` Micro-pitch |
| 12 | Velvet diffuse | `spread_amount` 0, `velvet_amount` 100 |
| 13 | Velvet, true decorrelation | `spread_amount` 0, `velvet_amount` 100, `mid_blend` 100 |
| 14 | Scene: Adaptive | `engine` Full, `spread_amount` 0, `pan_amount` 100, `velvet_amount` 60, `velvet_source` Noise, `transient_duck` 70 |
| 15 | Scene: Vocal | `engine` Full, `spread_amount` 0, `mod_amount` 60, `mod_type` Micro-pitch, `mod_source` Tonal, `velvet_amount` 50, `velvet_source` Noise, `transient_duck` 80, `bass_mono_hz` 180 |
| 16 | Scene: Ensemble | `engine` Full, `spread_amount` 30, `spread_source` Noise, `pan_amount` 100, `pan_depth` 90, `pan_max_groups` 8, `velvet_amount` 40, `velvet_source` Noise |
| 17 | Scene: Stable partials | `engine` Full, `spread_amount` 0, `pan_amount` 100, `pan_mode` Tracks |

## 9. User interface

The editor is one fixed window of 980 by 640 points, built from standard JUCE components with a custom `LookAndFeel`, showing every parameter and three live displays.

### 9.1 Layout

```text
+--------------------------------------------------------------------------------+
| stereophield   [preset menu  v]   Engine [Light|Full]   Latency: 0 ms  [Bypass] |  header, 48 pt
+---------------------------------------------------------------+----------------+
| SPREAD      | DELAY       | MOD         | VELVET     | PAN MAP |                |
| (o) amount  | (o) amount  | (o) amount  | (o) amount | (o) amt |   GONIOMETER   |
| * awake     | * asleep    | * asleep    | * asleep   | * asleep|   260 x 260    |
| source [v]  | source [v]  | source [v]  | source [v] | mode [v]|                |
| type   [v]  | time        | type   [v]  | size       | depth   +----------------+
| time        | low-pass    | rate depth  | density    | density | correlation    |
| density     | side [v]    | base        | variation  | bass ctr| -1 ====|==== +1|
| lo hi skew Q|             | cents predly|            | groups  | L [=====    ]  |
|                                                               | R [=====    ]  |
+---------------------------------------------------------------+----------------+
| PAN MAP DISPLAY: pan (vertical, L top, R bottom) against frequency (log)        |  120 pt
+--------------------------------------------------------------------------------+
| ANALYSIS      | SIDE BUS                                   | OUTPUT             |
| ambience      | bass mono   xover lo   xover hi            | (O) WIDTH          |
| room decay    | low  mid  high   duck   guard [on]         | mid blend          |
|               |                                            | comp [v] gain      |
|               |                                            | listen [S|M|Side]  |
+--------------------------------------------------------------------------------+
```

### 9.2 Behaviour

- Every control is bound to its parameter with an APVTS attachment. No control holds state of its own.
- Each generator card shows a small indicator: lit when awake, dim when asleep. It reads an atomic flag written by the `SleepController`.
- In the Light engine, the Pan map card, the Analysis panel and all source selectors are disabled and drawn at 40 % opacity.
- The latency readout shows the reported latency in milliseconds.
- Controls that do not apply to the selected type are hidden, for example `spread_time_ms` when `spread_type` is Cascade.
- `mid_blend` above 0 shows the label "not mono-safe" beside the control.
- Double-click resets a control to its default. Values are shown with units.

### 9.3 Live displays

| Display | Data | Drawing |
| --- | --- | --- |
| Goniometer | `(L, R)` pairs from the FIFO | Plot each pair at `x = (R - L) / sqrt(2)`, `y = (L + R) / sqrt(2)`, as dots that fade over 150 ms |
| Correlation meter | Correlation value | Horizontal bar from -1 to +1 with a centre mark; the negative half uses the warning colour |
| Level meters | Peak per channel | Two bars, -60 to +6 dB, with a 1.5 s peak hold |
| Pan map display | 128 points from section 6.3.5 | Dots at pan against log frequency, brightness by magnitude. Shows "Full engine only" in the Light engine |

Refresh all displays from one 30 Hz `juce::Timer`. Do not use OpenGL.

### 9.4 Visual style

| Element | Value |
| --- | --- |
| Background | `#14161A` |
| Panel | `#1D2026`, 8 pt corner radius |
| Text | `#E6E8EB`, secondary `#8B919A` |
| Accent | `#4FD1C5` |
| Warning | `#F56565` |
| Font | JUCE default sans-serif, 13 pt body, 11 pt labels |
| Knobs | Rotary, 270 degree arc, value arc in the accent colour |

### 9.5 Checking the interface

Add a test that constructs the editor, calls `createComponentSnapshot` and writes `docs/ui.png`. Open the image and inspect it. Fix any overlapping, clipped or unlabelled control, then regenerate it. Repeat with the engine set to Full.

## 10. Tests and measurements

The product is complete only when the 28 tests below pass; each has a numeric criterion, and the measured value goes into `docs/MEASUREMENTS.md`.

### 10.1 Harness

- **`sph_tests`** tests classes in `src/dsp/` directly. **`sph_plugin_tests`** drives the full `AudioProcessor` offline: it sets parameters, calls `prepareToPlay` and feeds blocks to `processBlock`.
- Unless a test says otherwise: sample rate 48 kHz, block size 512, mono input, 1 s of warm-up audio discarded before measuring.
- After `prepare()`, `reset()` or a preset load, every smoother starts at its target value. Tests rely on this.
- Levels in dB are relative to the RMS of the input signal unless stated.
- Modules expose read-only test hooks where a test needs internal values (masks, track lists, group lists, current delay times). Hooks are compiled in all builds and cost nothing when unused.
- **`sph_measure`** runs every measurement, rewrites `docs/MEASUREMENTS.md`, and renders each factory preset on the `mix` signal to `build/renders/<preset>.wav` for the owner to listen to. Renders are not committed.

### 10.2 Test signals

All are generated in `tests/signals/TestSignals.h` with fixed seeds.

| Name | Definition |
| --- | --- |
| `impulse` | One sample of amplitude 0.25, then zeros |
| `noise` | White Gaussian noise, RMS -12 dBFS, seed 1 |
| `sine(f)` | Sine at frequency `f`, amplitude 0.25 |
| `clicks` | One sample of amplitude 0.5 every 250 ms |
| `decayTone` | 440 Hz sine, steady for 1 s, then exponential decay of 60 dB per second for 1 s |
| `twoSource` | Source A: 220 Hz with harmonics 1 to 8 at amplitude `1/h`, starting at 0.5 s. Source B: 277.18 Hz, same recipe, starting at 1.0 s. 10 ms fade-ins. Peak normalised to 0.5. Length 3 s |
| `melody` | Four notes at 220, 247, 262, 294 Hz, each with harmonics 1 to 8 at `1/h`, 300 ms long, 5 ms fades, 20 ms gaps |
| `toneClick` | 440 Hz sine at -18 dBFS with one click of amplitude 0.9 at 1.0 s |
| `gapNoise` | `noise` for 2 s, silence for 3 s, `noise` for 2 s |
| `mix` | `twoSource` plus `noise` at -30 dB plus `clicks` at -12 dB |

### 10.3 Test list

**Fixed** criteria may never be relaxed. **Tunable** criteria may be met by adjusting the constants marked tunable in section 6; if they still fail, section 11.3 applies.

| ID | What is tested | Setup | Pass criterion | Kind |
| --- | --- | --- | --- | --- |
| T1 | STFT identity | `Stft` alone, `noise`, at 44.1, 48 and 96 kHz | Output minus input delayed by the measured latency is at most -100 dB | Fixed |
| T2 | Mono-safe invariant | Every preset with `mid_blend` forced to 0, plus 20 random parameter sets with `mid_blend` 0 and `comp_mode` Mono-exact; `mix`; at 44.1, 48 and 96 kHz | `(L + R) / 2` minus `out_gain * M_d` is at most -120 dB | Fixed |
| T3 | Reported latency | `impulse`, `width` 0, both engines, three sample rates | Index of the peak of `(L + R) / 2` equals `getLatencySamples()`; Light engine gives 0 | Fixed |
| T4 | Spread power is flat | Spread only, amount 100, `bass_mono_hz` 20, duck 0, guard Off; `impulse`; 65536-point FFT; both types | `abs(L)^2 + abs(R)^2` is flat within 0.1 dB from 20 Hz to 20 kHz | Fixed |
| T5 | Spread is complementary | Same as T4 | Pearson correlation between `abs(L)^2` and `abs(R)^2` over 400 log-spaced points from 50 Hz to 15 kHz is at most -0.9 | Fixed |
| T6 | Authentic Haas | Preset 2, `noise` | `L` minus input at most -100 dB. `R` minus input delayed 720 samples at most -60 dB | Fixed |
| T7 | Chorus anti-phase | `Mod` alone, Chorus | `dL + dR` equals `2 * mod_base_ms` within 0.0001 ms at every sample; output has no NaN | Fixed |
| T8 | Micro-pitch accuracy | `Mod` alone, Micro-pitch, 9 cents, `sine(1000)` for 10 s | Peak frequency of `pL` is 1005.21 Hz and of `pR` is 994.81 Hz, each within 0.58 Hz | Fixed |
| T9 | Velvet decorrelation | `Velvet` alone, `noise` for 10 s | Zero-lag correlation of `vL` and `vR` has magnitude at most 0.25. RMS of each within 1 dB of input. Third-octave levels from 125 Hz to 16 kHz within 5 dB of input | Tunable |
| T10 | Component buses sum to input | Full analysis, `mix`, with the test-only transient synthesis | Tonal + transient + noise minus input delayed by `Lat` at most -90 dB | Fixed |
| T11 | Split quality | Full analysis, `ambience` 0 | `sine(440)`: tonal bus within 1 dB of input, others at most -20 dB. `clicks`: transient bus at least 10 dB above each other bus. `noise`: noise bus at least 3 dB above each other bus | Tunable |
| T12 | Ambience split | `decayTone`, `ambience` 100, `room_decay_s` 1.0 | Magnitude-weighted mean of `ma` is at most 0.1 from 0.3 to 1.0 s and at least 0.3 from 1.1 to 1.8 s | Tunable |
| T13 | Source grouping | `twoSource`, Pan map only, Groups, amount 100, depth 100, duck 0, guard Off. Evaluate 1.5 to 2.5 s | Exactly 2 active groups. At least 12 of 16 partials in the correct group. Group pans have opposite signs. Summed harmonic energy of A differs by at least 6 dB between `L` and `R`, and B differs by at least 6 dB the other way | Tunable |
| T14 | Melody stays in place | `melody`, same setup as T13 | All four notes receive the same pan value | Tunable |
| T15 | Transient centring | `toneClick`, Spread only, `transient_duck` 100 against 0 | Full engine: side energy from 1 ms before to 5 ms after the click is at least 20 dB lower. Light engine: at least 10 dB lower from 0.5 to 5 ms after | Tunable |
| T16 | Correlation guard | All generators at amount 100, `width` 200, guard On, `noise` | Correlation over every 100 ms window after the first 500 ms is at least -0.1 | Tunable |
| T17 | Smart disable changes nothing | Preset 16. Part 1: `gapNoise`. Part 2: `noise`, with each of the Spread, Delay, Mod and Velvet amounts taken to 0 for 1 s and back | Part 1: output with and without `forceAwake` differs by at most -100 dB over the whole render. Part 2: at most -80 dB from 150 ms after each wake | Fixed |
| T18 | Smart disable saves work | Preset 16, 20 s renders | Silent input takes at most 0.2 times the time of `noise`. All amounts 0 with `noise` also takes at most 0.2 times | Tunable |
| T19 | No allocation on the audio thread | Test build replaces global `operator new` with a counter that is active inside `processBlock`; run presets 1, 14 and 16 with parameter automation | Count is 0 | Fixed |
| T20 | No denormals | `forceAwake` on, `noise` for 1 s then silence for 5 s, preset 16 | No output sample has magnitude strictly between 0 and 1e-30 | Fixed |
| T21 | Robustness | 200 random parameter sets, block sizes 16, 64, 512 and 1024, three sample rates, `mix` | No NaN or infinity; output peak at most 4 times input peak | Fixed |
| T22 | Block-size invariance | Presets 1 and 14, `mix`, block size 32 against 1024 | Difference at most -100 dB | Fixed |
| T23 | State round trip | Random parameter set, save state, load into a new instance | Every parameter equal; rendered output identical | Fixed |
| T24 | Audio Unit validation | `auval -v aufx Stph Hgda` | Ends with `AU VALIDATION SUCCEEDED` | Fixed |
| T25 | Performance | Preset 16, 60 s of `mix`, block 512; and preset 1 | Full engine render takes at most 4.8 s (8 % of real time). Preset 1 takes at most 0.9 s | Tunable |
| T26 | Preset sanity | Every preset, 5 s of `mix` | No NaN; peak at most 4 times input peak; presets with `mid_blend` 0 also pass T2 | Fixed |
| T27 | Bypass alignment | `bypass` on, both engines, stereo `noise` input | Output minus input delayed by `Lat` at most -120 dB | Fixed |
| T28 | Interface snapshot | Section 9.5 | `docs/ui.png` exists for both engines, is 980 by 640 points, and has been inspected | Fixed |

### 10.4 Notes on specific tests

- **T2** is the most important test in the project. Run it after every change to the side bus or output stage.
- **T8** is a module-level test because the plugin output mixes the dry signal with both shifted voices.
- **T13** allows four wrong partials because some harmonics of the two sources fall in the same FFT bin and cannot be separated.
- **T17 part 2** requires that `advanceWhileAsleep` keeps the smoothed chorus LFO value current, not only its phase. The simplest way is to run the LFO and its one-pole alone, per sample, while asleep.
- **T17** excludes a zero-gain sleep of Pan map, because its tracking state legitimately differs after a reset. For that case only check T2 and T21.
- **Timing tests (T18, T25)** must run in a Release build. Take the median of five runs.

## 11. Build order, gates and completion

Build in ten phases; each ends with a gate, a commit and a push, and the run ends only when the checklist in 11.4 is complete.

### 11.1 Phases

| Phase | Build | Gate (all must pass, plus every earlier gate) |
| --- | --- | --- |
| 0. Bootstrap | Toolchain check. Save this document as `docs/DESIGN.md`. CMake project with JUCE and Catch2. A pass-through plugin that accepts mono or stereo input and has stereo output | Builds AU and Standalone; T24; one trivial Catch2 test runs under CTest |
| 1. Foundations | Building blocks of section 5.4, test signals, `Stft` | T1; unit tests for `DelayLine` (integer delay exact; fractional delay of a sine has the right phase) and `OnePole` |
| 2. Core chain | Input split, alignment delay, side bus, output stage, Spread, Delay, and the parameters they need | T2, T3 (Light), T4, T5, T6, T16, T27 |
| 3. Mod and Velvet | Both Mod types, Velvet | T7, T8, T9, T2 |
| 4. Analysis | Transient detector, Full engine, stages A and B, source routing, latency switching | T3 (Full), T10, T11, T12, T15, T2 |
| 5. Pan map | Static, then Tracks, then Groups, with the display data | T13, T14, T2 |
| 6. Smart disable | `SleepController`, cascading, tail reporting, `forceAwake` | T17, T18, T19, T20 |
| 7. Plugin completion | All 50 parameters, 17 presets, state, programs, bypass | T21, T22, T23, T24, T25, T26, T27 |
| 8. User interface | Everything in section 9 | T28, T24 |
| 9. Release | Full test run, `sph_measure --all`, README, tag | Section 11.4 |

### 11.2 Routine at every gate

1. Build in Release with no warnings from your own code.
2. Run `ctest`. All tests from this and earlier phases must pass.
3. Update `docs/MEASUREMENTS.md` with the measured values.
4. Commit with the message `phase N: <summary>` and push.

### 11.3 Fallbacks

Use these only after three different attempts to pass a tunable test. Log each one in `docs/DECISIONS.md` with the measured value, then continue.

| Failing test | Fallback |
| --- | --- |
| T13 or T14 | Make Tracks the default `pan_mode`. Label the third option "Groups (experimental)" in the interface. Switch presets 14 and 16 to Tracks |
| T12 | Set the default of `ambience` to 0, in the parameter and in every preset |
| T11 | Keep the best constants found. The masks still sum to 1, so T10 and T2 are unaffected |
| T9 | Keep the best settings found |
| T15 | Keep the best detector constants found |
| T16 | Shorten the guard windows from 200 ms to 100 ms and re-measure. Keep the better result |
| T18 or T25 | Record the measured times |
| JUCE build errors | Pin JUCE tag `8.0.12` |

Fixed tests have no fallback. If a fixed test fails, the code is wrong; find and fix the cause.

### 11.4 Definition of done

- [ ] `cmake --build` succeeds in Release for arm64 with no warnings from project code.
- [ ] All fixed tests T1 to T28 pass.
- [ ] Every tunable test either passes or has its fallback applied and logged.
- [ ] `auval -v aufx Stph Hgda` succeeds on the final build.
- [ ] The Standalone application launches and passes audio.
- [ ] All 50 parameters exist with the identifiers, ranges and defaults of section 8.
- [ ] All 17 presets load and pass T26.
- [ ] The interface matches section 9, and `docs/ui.png` has been inspected for both engines.
- [ ] `docs/DESIGN.md`, `docs/DECISIONS.md`, `docs/MEASUREMENTS.md` and `README.md` are complete.
- [ ] The final commit is tagged `v1.0.0` and pushed with its tag, or the push failure is logged.

### 11.5 Final report

When done, print a short summary: the JUCE tag used, the test results as passed, fallback or failed counts, the reported latency of each engine at 48 kHz, the measured performance figures, and every fallback applied.

## 12. References

Every algorithm is fully specified above, so these sources are background only; do not block on reading them. The paper citations are given from memory and are approximate.

| Topic | Source |
| --- | --- |
| Framework | [JUCE repository and releases](https://github.com/juce-framework/JUCE) |
| Test framework | [Catch2 repository](https://github.com/catchorg/Catch2) |
| Delay-based complementary comb (section 6.3.1, Delay type) | H. Lauridsen, 1954; M. R. Schroeder, "An artificial stereophonic effect obtained from a single audio signal", JAES, 1958 |
| All-pass complementary comb (section 6.3.1, Cascade type) | R. Orban, "A rational technique for synthesizing pseudo-stereo from monophonic sources", JAES, 1970 |
| Shaped frequency-dependent spreading | M. Gerzon, "Signal processing for simulating realistic stereo images", AES 93rd Convention, 1992 |
| Decorrelation theory | G. Kendall, "The decorrelation of audio signals and its impact on spatial imagery", Computer Music Journal, 1995 |
| Velvet-noise decorrelation (section 6.3.4) | B. Alary, A. Politis, V. Välimäki, "Velvet-noise decorrelator", DAFx, 2017 |
| Median-filter component split (section 6.2.3) | D. Fitzgerald, "Harmonic/percussive separation using median filtering", DAFx, 2010; J. Driedger, M. Müller, S. Disch, "Extending harmonic-percussive separation of audio signals", ISMIR, 2014 |
| Late-reverberation estimate (section 6.2.4) | K. Lebart, J. M. Boucher, P. N. Denbigh, "A new method based on spectral subtraction for speech dereverberation", Acta Acustica, 2001 |
| Partial tracking (section 6.2.5) | R. McAulay, T. Quatieri, "Speech analysis/synthesis based on a sinusoidal representation", IEEE Trans. ASSP, 1986 |
| Grouping cues (section 6.2.6) | A. Bregman, *Auditory Scene Analysis*, MIT Press, 1990 |
| Transient handling in synthetic stereo | J. Breebaart et al., "Parametric coding of stereo audio", EURASIP Journal on Applied Signal Processing, 2005 |
