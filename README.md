# stereophield

A mono-to-stereo widening Audio Unit and Standalone app for macOS on Apple Silicon, built from classical signal processing only. By default its output sums back to the untouched input in mono: every algorithm adds to the side signal, and the mid is only delayed, never filtered. The interface is in English and Czech and explains itself as you use it.

It opens in **Easy mode**: one Width knob and three to shape it. Full controls opens the complete instrument underneath, sounding exactly the same.

- Specifications: [docs/DESIGN.md](docs/DESIGN.md) (1.0), [PART2_LEDGER.md](PART2_LEDGER.md) (2.0) and [PART3_LEDGER.md](PART3_LEDGER.md) (3.0, Easy mode)
- Decisions, deviations and tuning: [docs/DECISIONS.md](docs/DECISIONS.md)
- Every measured value: [docs/MEASUREMENTS.md](docs/MEASUREMENTS.md); per-preset metrics: [docs/PRESET_METRICS.md](docs/PRESET_METRICS.md); research: [docs/RESEARCH.md](docs/RESEARCH.md)

![stereophield 3.0, Easy mode](docs/ui-easy/easy-en.png)

## Easy mode

| Control | What it does |
| --- | --- |
| **Width** | Perceived width, from mono to as wide as stays natural; even steps of the knob give even steps of width |
| **Character** | How the width is made: clean (phase, coherence) at 0 %, diffuse (decorrelation) at 50 %, lively (double-tracking, micro-pitch) at 100 % |
| **Space** | Early reflections of a virtual room, and a more widely spaced virtual microphone pair |
| **Focus** | Keeps the centre: bass mono, transient ducking, a narrower low band |
| **Adapt to material** | The mapping follows what it hears (percussive, tonal or mixed) |
| **Low latency** | The Light engine and its own curves, without latency |

Each macro drives several internal parameters along curves that `sph_easy_opt` fitted by measurement: the corpus is rendered through the real engine, and the curves make perceived width (a virtual listener at loudspeakers) rise evenly with Width, while every setting stays mono-exact, keeps the correlation positive, adds at most 1.5 LU of loudness and keeps the bass centred. Under the hood shows the internal values live. Automating a macro writes no automation for the internal parameters. **Full controls** expands to the complete interface with exactly the values Easy mode was using; going back asks first, since it replaces hand-made settings (one undo step returns them). Sessions from 1.0 and 2.0 open in Complete mode, unchanged.

![stereophield, complete interface](docs/ui-full.png)

## What it does

Eight generators each synthesise a stereo difference signal; a ninth reshapes an existing stereo image. They run in parallel and sum into one side bus.

| Generator | Technique |
| --- | --- |
| Spread | All-pass side signal: a delay (Lauridsen) or an all-pass cascade (Orban comb, shaped spread) |
| Delay | Haas delay: a mono-safe half-depth comb, or the authentic dry/delayed pair with mid blend |
| Mod | Dimension-style anti-phase chorus, or a micro-pitch doubler |
| Velvet | Velvet-noise decorrelator, with offline-optimised sequences |
| Pan map | Spectral panning by frequency, by partial, or by source (Full engine) |
| Coherence designer | You choose how similar left and right are at each frequency, or take it from a virtual microphone pair (AB, XY, Blumlein, ORTF); mono-safe by construction (Full engine) |
| Double-tracker | Two synthetic takes with humanised timing, pitch, level and tone drift (artificial double tracking) |
| Room cues | Early reflections of a virtual room heard by a virtual ORTF pair, no reverb tail |
| Image expander | For stereo input: moves each panned source outward and stops it at the speaker (Full engine) |

Two engines: **Light** has no latency; **Full** analyses the spectrum (tonal, noise and transient parts, partial tracking, source grouping, a spectral-flux transient detector) with one FFT frame of latency (2048 samples at 48 kHz). With latency mode **Always Full**, switching engine is seamless.

The side bus has bass mono, three width bands, transient ducking and a correlation guard. Smart disable stops computing any module that cannot affect the output, without changing the sound. `mid_blend` at 0 keeps the mono sum identical to the input; above 0 the interface says "not mono-safe".

29 factory presets cover the classic techniques, virtual microphone pairs, double-tracking, room cues and scenes. User presets are files in `~/Library/Audio/Presets/highda/stereophield`.

### Learning with it

- **Info panel** (the `i` button): hover anything to see what it does, a live explanation of the current value (for example the comb spacing of the current delay), how it works, something to try, and the background.
- **Learn**: six guided tours (mid and side, mono compatibility, Haas and comb filters, decorrelation, spectral panning, smart disable). Apply loads a demonstration as one undoable step.
- **Teaching sources** (the `...` menu): noise, two sources, a melody, a tone with a click, a drum loop, so you can learn without material.
- **Views**: a live signal-flow diagram, scope lanes for every signal path with an activity timeline of smart disable, per-band correlation and mono-fold deviation, the coherence target and the achieved coherence, the pan map, a goniometer, and a perceived-width meter (a virtual listener on loudspeakers or headphones).
- Undo and redo, A/B compare, zoom from 75 to 150 %, keyboard access, and screen-reader names in both languages.

## Requirements, build and test

- macOS 12 or later on arm64; Command Line Tools (`xcode-select --install`); CMake 3.22 or newer; Ninja
- JUCE 9.0.3 and Catch2 v3.16.0 are fetched by CMake.

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
ctest --test-dir build --output-on-failure
./build/sph_measure --all --out docs/MEASUREMENTS.md     # every measurement, plus interface snapshots
./build/sph_measure --report build/report                # listening report: open build/report/index.html
./build/sph_easy_opt --optimise --target 0.40 --write    # refit the Easy mode curves (src/plugin/EasyTables.h)
```

The build installs the Audio Unit to `~/Library/Audio/Plug-Ins/Components/stereophield.component`. Validate it with:

```bash
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aufx Stph Hgda
```

The Standalone app is `build/stereophield_artefacts/Release/Standalone/stereophield.app`.

| Item | Value |
| --- | --- |
| Type, subtype, manufacturer | `aufx`, `Stph`, `Hgda`, version 3.0.0 |
| Channels | mono or stereo in, stereo out |
| Latency at 48 kHz | Light 0; Full 2048 samples (42.7 ms); Always Full 2048 in both engines |
| CPU at 48 kHz, M3 | preset 16 about 2.6 % of real time; every generator, Full engine and every view about 3.6 % |
| Easy mode at defaults | Full engine, 2048 samples of latency (0 with Low latency), about 3.1 % of real time |
| Sessions from 1.0 and 2.0 | load in Complete mode and render bit-identically |

---

## Česky

stereophield je plugin Audio Unit a samostatná aplikace pro macOS na Apple Silicon, která z mono signálu dělá stereo pouze klasickým zpracováním signálu. Ve výchozím stavu se jeho výstup v mono sečte zpět přesně na nedotčený vstup: všechny algoritmy přidávají jen do boční složky a střed se pouze zpožďuje, nikdy nefiltruje. Rozhraní je v češtině a angličtině (přepínač EN | CZ v záhlaví) a samo vysvětluje, co děláte.

**Snadný režim.** Plugin se otevře se čtyřmi makry: **Šíře** a k jejímu dotvarování **Charakter** (čistá, rozptýlená nebo živá šíře), **Prostor** (časné odrazy a širší virtuální mikrofonní pár) a **Soustředění** (basy, údery a spodní pásmo zůstávají uprostřed); dále **Přizpůsobit materiálu** a **Nízká latence**. Každé makro posouvá více vnitřních parametrů po křivkách, které byly nalezeny měřením: vnímaná šíře roste s knoflíkem rovnoměrně a každé nastavení zůstává mono-kompatibilní, s kladnou korelací, nanejvýš o 1,5 LU hlasitější a s basy uprostřed. Panel Pod kapotou ukazuje vnitřní hodnoty živě. Tlačítko **Všechny prvky** otevře úplné rozhraní se stejnými hodnotami a stejným zvukem; návrat do snadného režimu se nejprve zeptá. Projekty z verzí 1.0 a 2.0 se otevřou v úplném režimu beze změny.

**Co obsahuje.** Osm generátorů syntetizuje rozdílový stereo signál: rozprostření (Lauridsen, Orbanův hřeben), Haasovo zpoždění, chorus a mikrotransponování, sametový šum, panoramatická mapa, **návrhář koherence** (zvolíte, jak podobné mají být kanály na každém kmitočtu, nebo vezmete křivku virtuálního mikrofonního páru AB, XY, Blumlein či ORTF), **zdvojovač** (dva syntetické „výkony“ s lidským kolísáním) a **prostorové odrazy** (časné odrazy virtuální místnosti bez dozvuku). Devátý, **rozšíření stereobáze**, posouvá zdroje stereo nahrávky ven až k reproduktorům.

**Učení.** Informační panel (tlačítko `i`) ukazuje při najetí myší co prvek dělá, živé vysvětlení současné hodnoty, jak to funguje, co vyzkoušet a zdroje. Tlačítko **Učení** nabízí šest prohlídek s průvodcem. V nabídce `...` jsou výukové zdroje (šum, dva zdroje, melodie, tón s luskem, bicí smyčka), zvětšení rozhraní a ukládání vlastních předvoleb. Zobrazení Tok signálu, Průběhy, Pásma, Koherence a Pan. mapa ukazují, co se děje uvnitř; měřič vnímané šíře simuluje posluchače u reproduktorů nebo ve sluchátkách.

**Sestavení** je stejné jako výše; plugin se nainstaluje do `~/Library/Audio/Plug-Ins/Components`. Projekty z verzí 1.0 a 2.0 se načtou a zní bitově stejně.
