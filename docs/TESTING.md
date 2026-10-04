# Testing and measurements

Every behaviour that can be measured has a test with a numeric criterion. The criteria and the latest measured values are in [MEASUREMENTS.md](MEASUREMENTS.md); objective metrics for each factory preset are in [PRESET_METRICS.md](PRESET_METRICS.md).

## Running

```bash
./scripts/build.sh --test                 # build, then every test (about 4 minutes)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R "P3-"           # one group
./build/sph_plugin_tests "[P3-T6]"        # one test, Catch2 tags
```

Run the suite serially (plain `ctest`, as above). The CPU tests (T18, T25, P2-T10, P2-T18, P2-T37, P3-T10) time real renders, and a parallel `ctest -j` makes the other tests compete for the cores.

T24 runs `auval` on the installed component, which the build installs to `~/Library/Audio/Plug-Ins/Components`.

## Test IDs

| Prefix | Covers |
| --- | --- |
| T1 to T28 | The engine: STFT, the mono-safe invariant, latency, each original generator, the analysis stages, the guard, smart disable, real-time safety (no allocation, no denormals), robustness over random settings, block-size invariance, state round trips, auval, CPU, presets, bypass, the interface snapshot |
| P2-T1 to P2-T37 | Texts in both languages, the info panel and lessons, the views and scope taps, the virtual listener, the coherence designer, the double-tracker, room cues, the image expander, optimised velvet, seamless changes, constant latency, the flux detector, undo / A/B / user presets, session compatibility with 1.0, host groups, keyboard access, perceptual metrics, the whole-plugin CPU budget |
| P3-T1 to P3-T12 | Easy mode: mono safety, linear width, correlation, loudness, bass and transients, inaudible expansion, the collapse confirmation, macro automation, the material classifier, CPU, the interface, compatibility with 1.0 and 2.0 sessions |
| R3 | Auto-width, a research item |

Tests without an ID check smaller contracts (parameter IDs and ranges, defaults, program recall, the lessons and in-depth texts).

## Tools

| Command | What it does |
| --- | --- |
| `./build/sph_measure --all --out docs/MEASUREMENTS.md` | Runs every measurement and rewrites the table, plus the interface snapshots in `docs/` |
| `./build/sph_measure P3-T2 P3-T4` | Runs selected measurements |
| `./build/sph_measure --metrics` | Rewrites `docs/PRESET_METRICS.md` |
| `./build/sph_measure --parameters` | Rewrites `docs/PARAMETERS.md` from the parameter layout |
| `./build/sph_measure --report build/report` | Writes an HTML listening report: dry, processed and mono-fold players, a goniometer and band-correlation image and the metrics for each preset, and a blind A/B section |
| `./build/sph_measure --make-fixtures <dir>` | Saves 20 random sessions and the hashes of their renders |
| `./build/sph_easy_opt --optimise --target 0.40 --write` | Refits the Easy mode curves and rewrites `src/plugin/EasyTables.h` (about a minute on 8 cores) |
| `./build/sph_easy_opt --probe` | Classifier weights and metrics of the shipped table on the corpus |
| `./build/sph_easy_opt --references <folder>` | Sets the width target from the median width of the stereo WAV or AIFF files in a folder (add `--optimise --write`) |
| `./build/sph_velvet_opt` | Regenerates the optimised velvet sequences (`src/dsp/VelvetTables.h`) |

## Fixtures

`tests/fixtures` holds 20 sessions saved by 1.0.0 and `tests/fixtures/v2` 20 saved by 2.0.0, each with the 64-bit FNV-1a hash of its render of the `mix` test signal. P2-T30 and P3-T12 load them and require every render to be bit-identical, and the sessions to open in Complete mode.

## Known limits

Some criteria are marked tunable: they describe a target, and where the target is not physically reachable the best result is kept and guarded against regression. These are reported as "known limit" in MEASUREMENTS.md.

| Test | Target | Measured | Why |
| --- | --- | --- | --- |
| P2-T15 | Coherence designer within ±0.05 of the target in every band | Up to 0.145 on noise; up to 0.76 on the tonal test mix | A decorrelated copy of a few strong partials cannot have an arbitrary coherence in narrow bands; noise is close |
| P2-T16 | Spaced-pair curve within ±0.08 | 0.145 | Same reason, near the first zero of the sinc curve |
| P2-T25 | Optimised velvet within 1.5 dB per third-octave | 1.82 dB (random sequences: 18.3 dB) | Short sparse filters cannot be flat everywhere and uncorrelated at once |
| P2-T29 | Soft bin ownership separates sources as well as Hard | Source A -5.4 dB against -6.1 dB | Blending shared bins gives part of each partial its neighbour's pan; Hard is the default, Soft an option against pan seams |
| R3 | Auto-width holds ASW 0.3 ± 0.05 in 95 % of windows | Mix 95.8 %, drum loop 67.5 % | Kick windows are mono below the bass-mono frequency at any width; a faster controller only chases the hits. Ships off by default, labelled experimental |
| P3-T2 | Width linear (R² ≥ 0.95) with ASW ≥ 0.35 at 100 % on every item | Met by every item without drums (R² 0.966 to 0.997, ASW 0.37 to 0.47; the mix saturates at 90 % and is 0.009 lower at 100 %). Drum loop R² 0.92, ASW 0.28; arrangement R² 0.87, ASW 0.32 | Kept centred on their hits, drums widen only at high side levels while a full arrangement saturates early; one open-loop mapping cannot linearise both |
| P3-T3 | Correlation ≥ 0 in every 100 ms window | Lowest -0.05 (drum loop, Character 0, Space 100 %, Focus 0) | The side's tails outlast a hit faster than the guard's 200 ms power average follows; within the guard bound of T16 (≥ -0.1) |

## Checks that need a person

Automated tests cannot judge these; they are part of every release:

- **Listening** on loudspeakers and headphones and in mono, with the HTML report and on your own material: does each preset and each Easy mode macro do what its name says, and does Width feel even?
- **Host test** in at least one Audio Unit host: automation of the macros and of engine parameters, expanding during playback, the collapse question, latency compensation with Always Full, session save and reload.
- **Standalone test**: audio device, teaching sources, window zoom.
- **Czech review** by a native speaker of every new or changed text.
