# Architecture

How stereophield works, from the signal graph to the interface. The parameter list is in [PARAMETERS.md](PARAMETERS.md); tests and measured results are in [TESTING.md](TESTING.md) and [MEASUREMENTS.md](MEASUREMENTS.md).

## Contents

1. [The mono-safe rule](#the-mono-safe-rule)
2. [Signal flow](#signal-flow)
3. [Engines and latency](#engines-and-latency)
4. [Analysis (Full engine)](#analysis-full-engine)
5. [Generators](#generators)
6. [Side bus and output](#side-bus-and-output)
7. [Smart disable](#smart-disable)
8. [Seamless changes](#seamless-changes)
9. [Easy mode](#easy-mode)
10. [State, sessions and presets](#state-sessions-and-presets)
11. [Interface](#interface)
12. [Source layout](#source-layout)
13. [References](#references)

## The mono-safe rule

The input is split into mid `M = (L + R) / 2` and side `S_in = (L - R) / 2`. The mid is only ever delayed (to align with the analysis latency), never filtered. Every algorithm adds to a separate synthesised side. With `mid_blend` at 0 and compensation Mono-exact, the output satisfies

```text
(L + R) / 2 = out_gain * M_delayed     exactly, for every parameter setting
```

because the synthesised side cancels in the sum. Test T2 checks this to -120 dB over 147 renders; Easy mode never sets `mid_blend` above 0.

Each generator returns two signals: `s`, its side contribution, and `m`, its mid difference, that is, what would have to be added to the mid for the generator's authentic, non-mono-safe sound (for a Haas delay: the comb that the dry/delayed pair produces in mono). `mid_blend` adds the summed `m` back; above 0 the interface shows "not mono-safe".

## Signal flow

```text
 input (mono or stereo)
   |
 [Input split]   M, S_in                               (mono input: S_in = 0)
   |
   +--> [Alignment delay, latency samples] --> M_d, S_in_d        never filtered
   |
   +--> [Transient detector]  envelope e(n) (both engines)
   +--> [Analysis]  Full engine: STFT -> Full, Tonal, Noise buses, partial tracks,
   |                source groups, onsets (spectral flux), pan map spectrum
   |
   |     each generator reads one bus, chosen by its source parameter
   |
   +--> Spread, Delay, Mod, Velvet, Double, Room      (both engines)
   +--> Pan map, Coherence designer                   (Full engine)
   |         S_bus = sum (amount * s),  D_bus = sum (amount * m)
   |
 [Side bus]   S_bus: bass-mono high-pass -> three width bands -> transient duck
   |                 -> width -> correlation guard -> S_syn
   |          D_bus: the same high-pass -> D_syn
   |
 [Image expander]  stereo input only, Full engine: remaps S_in_d per frequency
   |
 [Output]     M_out = M_d + mid_blend * D_syn
              S_out = S_syn + S_in_d
              L = M_out + S_out,  R = M_out - S_out
              -> loudness compensation -> output gain -> listen mode -> meters
```

Generators run in parallel and their contributions are summed; they are never chained, since chained decorrelators multiply their colouration. Each one can sleep on its own (see [Smart disable](#smart-disable)).

## Engines and latency

| Engine | What runs | Latency |
| --- | --- | --- |
| Light | Time-domain generators; every source reads the full mid | 0 |
| Full | Adds the STFT analysis, the spectral sources, Pan map, the coherence designer and the image expander | one FFT frame: 2048 samples up to 50 kHz, 4096 up to 100 kHz, 8192 above (42.7 ms at 48 kHz) |

The latency is reported through `setLatencySamples` from the message thread. With `latency_mode` Always Full, both engines report the Full latency: the dry path keeps its delay and only the Full-only parts fade in and out (over 30 ms) when the engine changes, so switching engines during playback moves nothing in time.

## Analysis (Full engine)

The STFT uses a square-root Hann window for analysis and synthesis at a hop of N / 4, and reconstructs the input exactly (T1: -138.8 dB). Each frame passes four stages, all using past frames only, so they add no latency beyond the frame:

| Stage | What it does |
| --- | --- |
| A, component split | Median filtering across frequency (17 bins) and time (9 frames) separates tonal, transient and noise parts; the three masks sum to 1 (after Fitzgerald) |
| B, ambience split | Energy that decays the way a room's reverberation would (`room_decay_s`) moves from the tonal to the noise part, scaled by `ambience` |
| C, partial tracking | Peaks of the tonal spectrum are refined by parabolic interpolation and matched frame to frame into up to 96 tracks |
| D, source grouping | Tracks that share a harmonic series and an onset become one source (up to `pan_max_groups`); a new source near the pitch of one that just ended inherits its position, so a melody stays in place |

The spectral sources Tonal, Noise and Tonal+Noise are resynthesised from the masked spectra; Full is the delayed mid.

**Transients.** The envelope detector compares a fast and a slow follower of the mid (threshold 2.2, range 0.6, 100 ms hold) and runs in both engines. In the Full engine with `transient_mode` Spectral flux, onsets come from the half-wave rectified spectral flux over a 16-frame median threshold, refined to the newest 3 hops with the envelope ratio, and are scheduled 2 ms ahead of the delayed signal, so ducking starts before the hit arrives.

## Generators

Every generator has an amount and (except Pan map) a source. All of them are mono-safe by construction.

| Generator | Side signal | Notes |
| --- | --- | --- |
| Spread | The input through an all-pass: a plain delay (Lauridsen) or a cascade of 2 to 24 second-order sections between two frequencies, with skew and Q (Orban) | L and R become complementary combs whose powers sum flat |
| Delay | Half the difference between the dry and a delayed copy (0.1 to 40 ms, optional low-pass) | Mid blend restores the authentic dry/delayed pair |
| Mod | Chorus: two delays modulated in opposite directions around a base, so their sum is constant. Micro-pitch: two pitch shifters at plus and minus a few cents, the right one 8 ms later | Lagrange-interpolated delay reads; the LFO keeps running while asleep |
| Velvet | Each side convolved with its own sparse sequence of +1 / -1 impulses (10 to 80 ms, 500 to 3000 per second, 16 variations) | The Optimised design uses sequence pairs found offline by `sph_velvet_opt` (flat to about 2 dB per third-octave, correlation near 0) |
| Pan map | Spectral panning, coherent with the dry: Static (a sine of log frequency), Tracks (each partial keeps its pan), Groups (each source gets one position) | Bins below `pan_bass_center_hz` stay centred; Hard or Soft bin ownership between neighbouring tracks |
| Coherence designer | `L = M + aD`, `R = M - aD`, where `D` is a decorrelated copy orthogonalised against `M` and `a` is solved per merged ERB band from measured powers, so the inter-channel coherence follows a target | Targets: a five-point curve, or the coherence of a spaced pair, a coincident pair (XY, Blumlein) or a near-coincident pair (ORTF); transient protection lowers decorrelation on onsets |
| Double-tracker | Two synthetic takes, each reading the input at its own slowly drifting delay (offset, drift amount and rate, pitch, level and tone drift, 16 seeds) | Drift is generated at fs / 32 and interpolated; no repeating cycle |
| Room cues | Early reflections of a rectangular room (image sources, first and second order, up to 24 within 80 ms) picked up by a virtual ORTF pair placed off-centre (40 % of the width, 45 % of the depth) | No reverb tail; a symmetric layout would cancel the side, which the off-centre pair avoids |
| Image expander | For stereo input: per bin, a panning index and a diffuseness from smoothed mid/side statistics; panned components move outward along a curve that stops at the speaker, the diffuse part is scaled separately, bins below a frequency are untouched | Remaps the input side instead of adding to the synthesised side |

## Side bus and output

1. **Bass mono:** a 24 dB/octave Linkwitz-Riley high-pass on the synthesised side (off at 20 Hz).
2. **Width bands:** Linkwitz-Riley crossovers split the side into low, mid and high bands with their own widths; the split only runs while the gains differ.
3. **Transient duck:** the side is reduced by `transient_duck * e(n)`.
4. **Width and guard:** the width gain, then the correlation guard: mid and side power over 200 ms; when the side would exceed `guard_ceiling_db` relative to the mid, the side is turned down (falls in 5 ms, recovers in 100 ms). At 0 dB this keeps the correlation at or above 0; at -6 dB at or above 0.6, which also bounds the loudness increase to about 1 dB.

The Pan map's side bypasses the time-domain filters: the same bass-mono and band responses are applied in its spectrum without phase shift.

The output stage forms L and R, applies loudness compensation (Mono-exact: none; Constant loudness: both channels scaled by sqrt (Pm / (Pm + Ps)) so the stereo loudness stays constant), the output gain and the listen mode (Stereo, Mono, Side). Bypass crossfades over 20 ms to the latency-aligned input.

**Auto-width** (`width_mode` Auto, experimental) runs the virtual listener on the output every 100 ms and moves the width toward `asw_target` (2 s time constant, at most 3 dB per second). It holds full mixes well but not percussive material; see [TESTING.md](TESTING.md#known-limits).

## Smart disable

A module that cannot affect the output is not computed: when its amount is zero, or when its input has been silent (below -140 dBFS) for longer than its tail. A sleeping module outputs zero and is reset once; modules with a phase (LFOs, drift) keep their phase running, so waking is seamless. The analysis keeps its input buffer running so it can wake on any hop. Sleep never changes the sound (T17: bit-identical to a render that never sleeps) and never changes latency. The Scopes view shows an activity timeline.

## Seamless changes

Parameters are smoothed. Structural changes of Spread, Delay, Mod, Velvet and the double-tracker, which alter a module's topology (for example the spread type, the mod type, the velvet size, the double-tracker seed, or a spectral source when the engine changes), use a pair of instances: the idle one takes the new structure, is pre-rolled from 150 ms of bus history so its state is already settled, and the two are crossfaded with a per-sample power correction, so there is no gap, bump or click (P2-T26).

## Easy mode

Easy mode is the default for new instances. Four macros (Width, Character, Space, Focus) and two switches (Adapt to material, Low latency) drive every engine parameter. The mapping runs in the processor before the engine reads each block's parameters, so automating a macro writes no automation for the engine parameters.

**Mapping** (`src/plugin/EasyMode.cpp`):

| Macro | Drives |
| --- | --- |
| Width | The side level, along a curve per material class, times a gain curve of Character |
| Character | Shares the side between three families with triangular weights: phase at 0 % (Spread; the coherence designer on percussive material), decorrelation at 50 % (optimised Velvet, the coherence designer's spaced pair), movement at 100 % (the double-tracker, micro-pitch); the pan map at a constant amount |
| Space | Room cues amount, room size and distance, the virtual pair's spacing |
| Focus | Bass mono 100 to 250 Hz, transient ducking (60 % at 0, 90 % at 50 %, 100 % at 100 %), a narrower low band, the coherence designer's transient protection |

Fixed in Easy mode: mid blend 0, guard on with a -6 dB ceiling, Mono-exact compensation, Stereo listen, spectral-flux detector, optimised velvet, high band at 120 %. The engine is Full with Always Full latency, or Light with Low latency (the coherence designer and the pan map then give way to velvet).

**Material classifier** (`src/dsp/MaterialClassifier.cpp`). Its own 1024-point spectrum at hop 512 (48 kHz), independent of the engine and without latency. Evidence: percussive onsets per second (spectral-flux peaks over an adaptive threshold where many more bins rise than fall) and the tonal share of energy (stable peaks 6 dB above the bins 3 to 8 away). With P and T mapped to 0..1, the weights are percussive P (1 - T), tonal T (1 - P), mixed the remainder, after a 2 s one-pole (at most 0.05 per 100 ms). Adapt off uses the mixed curves only.

**Curves** (`src/plugin/EasyTables.h`, written by `sph_easy_opt`). The corpus (`tests/common/Corpus.h`: noise, two sources, melody, tone and click, drum loop, mix, pad and melody, arrangement) is rendered through the real plugin, each item with its own classifier weights, over a grid of side level (0 to 200 %) and Character. Each metric becomes a measured function of side level. The curve control points of all three classes are then fitted to minimise

```text
J = sum over items, Width 0..100 % and Character 0..100 %:
      100 (ASW - Width x min (0.40, reachable ASW of the item))^2
    + 1000 max (0, 0.01 - ASW step)^2                       each Width step must widen
    + 1000 max (0, -minimum window correlation)^2
    + 50 max (0, loudness change - 1.5 LU)^2
    + 50 max (0, side/mid below 80 Hz + 12 dB)^2
    + curve smoothness
```

by a (1 + 16) evolution strategy followed by coordinate descent, from a linear start and 24 seeded random starts. ASW is the virtual listener at loudspeakers. `sph_easy_opt --references <folder>` sets the target from the median width of reference tracks.

**Expand and collapse.** Full controls writes the values the mapping is producing into every parameter (as one undo step) and switches to the complete interface; the render continues bit-identically (P3-T6). Returning to Easy mode asks for confirmation, because it replaces hand-made settings; one undo step brings them back.

## State, sessions and presets

- Every float parameter is an `ExactFloatParameter`, which keeps its exact plain value behind the normalised value hosts see, so saving, loading, undo and A/B restore values bit for bit.
- The saved state is the parameter tree plus the program, a state version (3) and the interface language. Parameters missing from an older state take the behaviour of the version that saved it: 1.0 states use the original algorithm choices and read the parameter mirror as 1.0 did; 1.0 and 2.0 states open in Complete mode. Sessions from 1.0 and 2.0 render bit-identically (P2-T30, P3-T12, 40 fixtures).
- Undo and redo keep up to 200 snapshots of every exact value: one after each finished gesture, around preset loads, state loads, lesson steps and mode changes. A/B holds two snapshots.
- 29 factory presets are settings of the complete interface; loading one switches to Complete mode. Recalling the current program on an untouched instance leaves it in Easy mode. User presets are XML files in `~/Library/Audio/Presets/highda/stereophield`.
- Parameters are in 14 host groups. Audio Unit hosts identify parameters by a hash of the ID, so grouping does not affect automation.

## Interface

The editor is 1240 x 800 points with the info panel open (980 x 800 closed), zoomable from 75 to 150 %.

- **Easy mode** shows the macros, a goniometer, correlation and perceived-width meters, the classifier's three weights and Under the hood: 15 internal values the macros are producing, live.
- **Complete mode** shows nine generator cards (amount, source, a mini scope, an awake indicator), the meter column, a centre strip with Details (the selected generator's controls and view), Pan map, Coherence, Flow, Scopes and Bands, and the analysis, side-bus and output panels.
- **Views.** Scope taps (24 signals, min/max columns at 400 per second) feed the card scopes, the scope lanes and the flow diagram, whose edges follow the level and whose asleep nodes are dimmed. The Bands view shows correlation and mono-sum deviation per third-octave (16384-point frames). The perceived-width meter is a Brown-Duda spherical head at ±30° loudspeakers or on headphones, giving 1 - IACC in the 500 Hz to 2 kHz octaves.
- **Info panel and learning.** Hovering anything shows its entry: what it does, a live explanation of the current value where useful, how it works, something to try, an in-depth section and references. Long entries scroll; the entry stays while the mouse is over the panel. Learn offers eleven lessons (how we hear width, mid and side, mono compatibility, a map of widening techniques, Haas and comb filters, decorrelation, microphone pairs, spectral panning, widening in a mix, Easy mode, smart disable); Apply sets up a demonstration as one undo step. Teaching sources (noise, two sources, a melody, a tone with a click, a drum loop) replace the input.
- **Language.** Every text comes from two tables (`src/ui/Strings.cpp`, `src/ui/HelpContent.cpp`, `src/ui/LearnContent.cpp`), English and Czech. Czech uses the terms common in Czech studio practice (bypass, presety, transienty, korelátor, mid/side) next to established Czech technical terms (hřebenový filtr, stereobáze, časné odrazy), with a decimal comma. The language is saved with the session and as a global preference; host-visible parameter names stay English.
- **Keyboard and accessibility.** Every control is focusable and named in both languages; Cmd-Z and Shift-Cmd-Z undo and redo.

## Source layout

| Path | Contents |
| --- | --- |
| `src/dsp` | The engine, without JUCE GUI: `Core` (the graph), analysis stages, generators, side bus, output, meters, scope taps, virtual listener, loudness, classifier, test signals |
| `src/plugin` | `AudioProcessor`, parameters, presets, the Easy mode mapping and its table |
| `src/ui` | Editor, Easy view, widgets, views, info panel, texts and lessons |
| `tests/unit` | Catch2 tests of the DSP building blocks |
| `tests/plugin` | Catch2 tests through the full processor and editor |
| `tests/common` | Shared measurements (one function per test ID), render harness, corpus, fixtures |
| `tests/measure` | `sph_measure`: runs every measurement, writes the docs, the listening report and fixtures |
| `tests/fixtures` | Sessions saved by 1.0 and 2.0 with the hashes of their renders |
| `tools` | `sph_velvet_opt` and `sph_easy_opt`, the offline optimisers that write `VelvetTables.h` and `EasyTables.h` |
| `scripts` | Build, install and packaging scripts |

## References

- Blumlein, A. D. (1931). British patent 394325 (stereo, coincident pairs, mid/side).
- Lauridsen, H. (1954); Orban, R. (1970). Pseudo-stereo by complementary combs and all-pass networks. JAES 18 (4).
- Haas, H. (1951). Über den Einfluss eines Einfachechos auf die Hörsamkeit von Sprache. Acustica 1.
- Schroeder, M. R. (1958). An artificial stereophonic effect obtained from a single audio signal. JAES 6 (2).
- Gerzon, M. A. (1992). Signal processing for simulating realistic stereo images. AES Convention 93.
- Cook, R. K. et al. (1955). Measurement of correlation coefficients in reverberant sound fields. JASA 27.
- Barron, M., Marshall, A. H. (1981). Spatial impression due to early lateral reflections in concert halls. J. Sound Vib. 77.
- Allen, J. B., Berkley, D. A. (1979). Image method for efficiently simulating small-room acoustics. JASA 65.
- Hidaka, T., Beranek, L., Okano, T. (1995). Interaural cross-correlation, lateral fraction, and low- and high-frequency sound levels as measures of acoustical quality in concert halls. JASA 98.
- Brown, C. P., Duda, R. O. (1998). A structural model for binaural sound synthesis. IEEE Trans. Speech Audio Proc. 6 (5).
- McAulay, R. J., Quatieri, T. F. (1986). Speech analysis/synthesis based on a sinusoidal representation. IEEE Trans. ASSP 34.
- Bregman, A. S. (1990). Auditory Scene Analysis. MIT Press.
- Fitzgerald, D. (2010). Harmonic/percussive separation using median filtering. DAFx.
- Lebart, K., Boucher, J.-M., Denbigh, P. N. (2001). A new method based on spectral subtraction for speech dereverberation. Acta Acustica 87.
- Bello, J. P. et al. (2005). A tutorial on onset detection in music signals. IEEE Trans. Speech Audio Proc. 13 (5).
- Avendano, C. (2003); Avendano, C., Jot, J.-M. (2004). Frequency-domain techniques for stereo to multichannel upmix; A frequency-domain approach to multichannel upmix. JAES 52.
- Breebaart, J. et al. (2005). Parametric coding of stereo audio. EURASIP J. Appl. Signal Proc.
- Elko, G. W. (2001). Spatial coherence functions for differential microphones in isotropic noise fields. In Microphone Arrays, Springer.
- Alary, B., Politis, A., Välimäki, V. (2017). Velvet-noise decorrelator. DAFx.
- Schlecht, S. J. et al. (2018). Optimized velvet-noise decorrelator. DAFx.
- Linkwitz, S. H., Riley, R. (1976). Active crossover networks for non-coincident drivers. JAES 24.
- ITU-R BS.1770 (loudness); IEC 60268-18 (correlation meters).
