# Decisions

Dated list of choices not dictated by `DESIGN.md`, and every fallback applied.

## 2026-10-03

- Repository initialised with `git init -b main`; public upstream `highda/stereophield` created with `gh repo create` and set as `origin`. `.gitignore` excludes `build/` and `.DS_Store`.
- First run stopped at the toolchain check (section 3.1): only the Command Line Tools were installed (`xcode-select -p` printed `/Library/Developer/CommandLineTools`, `xcodebuild` unavailable). No code written yet; phase 0 starts on the next run. Other checks passed: arm64, CMake 4.0.2, ninja present, `auval` present, `origin` set.
- Section 3.1 amended at the owner's request: full Xcode is no longer required. The Command Line Tools on this machine provide Apple clang 21, the macOS 26.5 SDK with AudioToolbox, AudioUnit, CoreAudio, CoreAudioKit, CoreMIDI and Accelerate, plus `codesign`, `Rez` and `auval`. The run stops only if no Apple toolchain is present or the phase 0 Audio Unit cannot be built.
