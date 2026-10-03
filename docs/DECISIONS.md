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
