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

### Phase 3

- `Rng::setSeed` scrambles the seed with the murmur3 32-bit finaliser before seeding xorshift32. Without it, seeds 1000 and 1001 (Velvet left and right) produced nearly identical first outputs, so both sequences put their dominant first impulse at the same position with the same sign; T9 measured a correlation of 0.371. With the scramble it measures 0.149. The `noise` signal changed accordingly; all earlier tests were re-run.
- Velvet draws `rnd1` and then `rnd2` from one generator per impulse, in impulse order. Positions are clamped to `[0, Len - 1]`.
- Velvet sequences are rebuilt on the audio thread inside the 20 ms fade instead of on the message thread: building two sequences of at most 240 impulses allocates nothing and costs a few microseconds, and it keeps offline renders deterministic and independent of the block size. The fade (out, rebuild, reset, in) is as specified.
- Mod's `reset()` clears only the delay line and parameter smoothers. The chorus phase, its smoothed LFO value and the pitch-shifter phasors start at zero in `prepare()` only, so that a module woken from sleep is in phase with one that never slept (T17). Each micro-pitch side advances by its own `|ratio - 1| / W`, with `ratio = 2^(+cents/1200)` on the left and `2^(-cents/1200)` on the right, which gives the 994.81 Hz of T8.
- Chorus rate is smoothed linearly over 20 ms and the LFO corner one-pole is recomputed from it once per sub-block; base, depth and pre-delay use the 50 ms delay-time one-pole.
- T7 is checked at three settings (defaults; 5 Hz with 5 ms depth; 0.05 Hz with base 3 ms and depth 2.5 ms, the deepest that does not hit the 0.5 ms clamp).

### Phase 4

- Stage A reset state equals the converged state after a long silence (every frame zero, so `r = 0` and every bin is transient): `mt_prev = 0`, `mx_prev = 1`, empty median history. This makes a slept-and-reset analysis identical to one that kept running on silence (T17).
- Frame time for T12 is the centre of the analysis window. T11 bus levels are RMS over 1 to 4 s of a 4 s render.
- Every STFT output channel is popped every sample, synthesised or not, so a skipped inverse transform never leaves stale samples in its accumulator.
- Sleep of a generator that reads a spectral bus is judged on the undelayed mid with `tail + 2N` samples of required silence, because a spectral bus at time `n` depends on the mid from `n - 2N + 1` to `n - 1`. Generators on the Full bus are judged on `M_d` with their own tail. This also avoids the circularity of judging a bus that is not computed while its consumer sleeps.
- The whole Full analysis sleeps on silence after `N` samples plus 0.5 s (section 7.1) rather than `N` samples (section 7.4): the inverse transforms still emit up to `N` samples of output after the input stops, and section 7.1 is the stricter of the two.
- The transient detector sleeps when `transient_duck` is 0 and settled, when every generator is asleep for zero gain, or when the undelayed mid has been silent for `Lat + 0.3 s`. Tying it to the side bus's sleep (section 7.4) would miss the onset in the Full engine, where the side bus wakes `N` samples after the mid does.
- A spectral generator's host tail is its own tail plus `N`.
- **T15 tuning (tunable constants of section 6.2.1).** With the specified thresholds 1.5 / 1.5 and 40 ms hold, T15 measured Full 17.6 dB (needs 20) and Light 19.1 dB. Probing the detector showed why: with the fixed follower times, `r = fast / slow` is about 2.0 on steady harmonic tones and 2.5 on steady noise, while the T15 click only reaches 2.92, so the spec constants both under-duck the click and duck steady material all the time (mean `e` 0.46 on `twoSource`). Approaches tried, measured as T15 Full / Light and T16 minimum: (1) hold 80 ms: 20.6 / 21.7 dB, -0.049; (2) threshold 2.5, range 0.4, hold 80 ms: 27.7 / 30.6 dB but T16 fell to -0.111, because sharper thresholds turn steady noise into bursts of ducking that the 200 ms guard estimates lag behind (T16 with ducking off stays within +-0.02); (3) a joint grid over T15 and T16. Chosen: **threshold 2.2, range 0.6, hold 100 ms**: T15 33.0 / 37.5 dB, T16 -0.071 with the specified 200 ms guard (no T16 fallback needed), mean `e` on `twoSource` 0.37 instead of 0.46.
- `flushLatencyUpdate()` on the processor sets the latency directly; without a message loop, `triggerAsyncUpdate()` cannot post and cancels itself.

### Phase 5

- **Pan map bypasses the time-domain side-bus filters (deviation from section 6.4).** With the specified order, the Pan map side signal went through the bass-mono LR4 high-pass and, when active, the band split, while `M_d` is never filtered. The high-pass shifts the side's phase by 122 degrees at 220 Hz and 44 degrees at 554 Hz (at its 150 Hz default), so amplitude panning `L = (1 + p) X`, `R = (1 - p) X` became frequency-dependent interference: T13 measured L-R differences of -0.2 dB (A) and 1.7 dB (B) with correct pans in every bin, and 220 Hz leaned the wrong way. The Pan map's side spectrum now gets the same magnitude responses as zero-phase per-bin weights: `|HP_LR4| = x^4 / (1 + x^4)` blended by the bass-mono crossfade, times `gL L1 + gM H1 L2 + gH H1 H2` with `L = 1 / (1 + x^4)` and `H = 1 - L` (which equals the common gain when the band gains are equal), computed once per block from the side bus's smoothed settings. The result joins `S_bus` after steps 1 and 2, so transient duck, width and guard still apply. T13 then measured -6.1 dB and +15.0 dB. The decorrelating generators keep the time-domain filters, where phase does not matter.
- Partial tracks visit peaks in bin order at birth; a track that misses a frame has amplitude 0 for that frame (it was not observed), which lets an ending note's group register as fading at once. A track's peak bin for ownership is the matched peak's bin, or `round(freq N / fs)` when it missed. Ownership boundaries are integer midpoints between neighbouring peak bins, ties going to the lower track.
- Grouping visits ungrouped tracks of age 2 or more in order of decreasing amplitude. Rule 1 and rule 3 share one search; rule 2 picks the smallest error. A group is freed in the frame after its last track dies, and its `f0`, `pan` and frame of death go into a 16-entry ring for voice continuity.
- **T14: a young group is re-placed when rule 2 gives it its fundamental.** On the `melody`, an upper partial of the next note (994 Hz, the 4th harmonic of 247 Hz) reached age 2 one frame before the fundamental and founded the group; at 26 semitones from the previous note, voice continuity could not see it and the group took a fresh slot (+1.0). When rule 2 replaces the `f0` of a group still inside its onset window (4 frames), `assignPan` now runs again with the new fundamental, ignoring the group itself. Tracks take their group's pan every frame. T14 measured -0.50 for all four notes; T13 was unchanged.
- The T13 group-membership count uses, for each of the 16 partials, the nearest track within 3 % and the group it belongs to in the majority of frames from 1.5 to 2.5 s; group A or B is the active group whose `f0` is within 3 % of 220 or 277.18 Hz. Harmonic energy is summed within +-3 Hz of each harmonic over 2^16 samples from 1.5 s plus the latency.
- Pan-mode changes fade the Pan map output, then switch and reset its tracking at the next block boundary.
- Display data is the smoothed pan and the masked magnitude at the bin nearest each of 128 log-spaced frequencies, published as relaxed atomics once per frame; the user interface weights brightness by magnitude.

### Phase 6

- A freshly reset `SleepController` counts as having seen an infinitely long silence, so a module can sleep from the first silent block. A module sleeps for silence only if its input was already silent for its required span before the block and the block itself is silent; the tail it was still producing is never cut.
- T17 part 2: preset 16 has Delay and Mod at 0 %, so taking them "to 0 and back" would not exercise their sleep, and Mod's `advanceWhileAsleep` would go untested. Part 2 starts from preset 16 with `delay_amount` and `mod_amount` at 50 %; each of the four amounts in turn goes to 0 for 1 s and returns to its starting value, with 1 s between events. Differences are measured as the energy of the left and right difference relative to the input.
- T17 measures exactly zero difference in both parts. With FTZ and deterministic double-precision state, the filters that were reset converge bit-for-bit to the never-slept state about 50 ms after a wake (the probe saw non-zero differences only in the first 20 ms), and after the 3 s gap every state has decayed to exact zero.
- T19 replaces global `operator new` and `operator delete` (all forms) in `sph_plugin_tests` and `sph_measure`; both allocations and frees count while the flag is set around `processBlock`. Automation moves one parameter per block, in turn, to a random value; `engine` is excluded because it is not automatable.
- Timing tests use `juce::Time::getMillisecondCounterHiRes` around offline renders in the Release build, median of five.

### Phase 7

- **T21 peak criterion is relative to the output gain.** `out_gain_db` reaches +12 dB (x 3.98) on its own, so "output peak at most 4 x input peak" with uniformly random parameters would fail on the gain knob rather than on the processing. The measured ratio is `output peak / (input peak x max(1, out_gain))`. The worst of 200 sets is 1.99.
- T21 runs 200 random sets in total, cycling the 12 combinations of block size (16, 64, 512, 1024) and sample rate (44.1, 48, 96 kHz), rather than 200 sets for each of the 12 combinations.
- **T23 equality uses JUCE's `approximatelyEqual` on the plain values the DSP reads.** For skewed ranges, `convertFrom0to1 (convertTo0to1 (x))` can come back one float step from `x`, and JUCE's state adapter deliberately ignores changes within that tolerance, so a reload can leave a parameter one step off with no way to correct it from outside. Five random sets reload with every parameter equal under that test and bit-identical rendered output. A host can also leave a choice, integer or bool at any normalised value (for example 0.82 for "on"); it reloads as the same step, which is why raw normalised values are not compared.
- T24 runs `auval` from `sph_measure` (after killing `AudioComponentRegistrar`) against the component installed by the build.
- A table test checks all 50 parameter IDs against section 8, with range ends, defaults, version hint 1, automatability (only `engine` is not automatable) and centre skew on frequency parameters. It also checks the 17 program names, the preset reset semantics, the bypass parameter and the bus layouts.

### Phase 8

- The editor lives in `src/ui/`: `Style` (colours, fonts, LookAndFeel), `Widgets` (knob with name and value, captioned combo box, segmented choice buttons, toggle, titled panel with awake indicator), `Displays` (goniometer, correlation meter, level meters, pan map display) and `PluginEditor`.
- Layout at 980 x 640: header 48, main row 316 (five generator cards of about 134 pt, and a 268 pt meter column with the 260 x 260 goniometer, correlation and level meters), pan map display 120, bottom row 132 (Analysis 150, Side bus, Output 352), 6 pt gaps.
- Choices that switch with two or three states in the header and output (`engine`, `listen`) are segmented buttons bound with `juce::ParameterAttachment`. `guard` and `bypass` are toggle buttons, and the other choices are combo boxes. `spread_density`, `velvet_variation` and `pan_max_groups` are knobs with integer steps.
- Hidden when they do not apply: `spread_time_ms` for Cascade; density, low, high, skew and Q for Delay; rate, depth and base for Micro-pitch; cents and pre-delay for Chorus; `pan_density` in Groups mode (it shapes the Static and Tracks curve); `pan_max_groups` outside Groups.
- Goniometer points are 1.5 pt dots with alpha falling linearly to zero over 150 ms; full scale (|L| = |R| = 1) reaches the circle. Level meters fall at about 24 dB per second between peaks; the portion above 0 dBFS is drawn in the warning colour. Pan map display brightness spans 60 dB below the loudest point.
- Slider value labels do not take keyboard focus by themselves; they edit on click. The editor calls `sendLookAndFeelChange()` after construction, because a slider whose text box was created before it joined the hierarchy otherwise kept the default LookAndFeel's box (first snapshot showed an outlined 15 pt box under WIDTH).
- T28: `docs/ui.png` is the Light engine on the default preset; `docs/ui-full.png` is the Full engine on preset 16. Both are rendered after 2.35 s of `mix` so that the displays show data. Inspected: the first snapshot had the Spread card's Q value clipped, a truncated "Mono-exact" and the outlined WIDTH box; all three are fixed in the committed images. A further inspection render (preset 2 with Full, Spread Delay type, Micro-pitch, Static pan, Side listen) confirmed the conditional controls, the "not mono-safe" warning and the Static curve on the pan display.
