# Measurements

Written by `sph_measure --all` on 2026-10-03. Release build, 48 kHz and block 512 unless a criterion says otherwise.

| Test | Criterion | Measured | Result |
| --- | --- | --- | --- |
| T1 | STFT identity: output minus input delayed by the measured latency <= -100 dB at 44.1, 48, 96 kHz | worst -138.8 dB; latency 2048, 2048, 4096 samples (N) | pass |
| T2 | Mono-safe: (L+R)/2 - out_gain * M_d <= -120 dB (every preset with mid_blend 0, 20 random sets; mix; 44.1, 48, 96 kHz) | worst -131.8 dB over 111 renders | pass |
| T3 | Peak of (L+R)/2 for an impulse with width 0 is at getLatencySamples(); Light gives 0 | Light 0 0 0; Full 2048 2048 4096 samples at 44.1/48/96 kHz | pass |
| T4 | Spread: \|L\|^2 + \|R\|^2 flat within 0.1 dB from 20 Hz to 20 kHz, both types | Delay 0.0000 dB, Cascade 0.0000 dB | pass |
| T5 | Spread: Pearson correlation of \|L\|^2 and \|R\|^2 at 400 log points 50 Hz - 15 kHz <= -0.9, both types | Delay -1.0000, Cascade -1.0000 | pass |
| T6 | Preset 2 on noise: L - input <= -100 dB; R - input delayed 720 samples <= -60 dB | L -138.5 dB, R -137.9 dB | pass |
| T7 | Chorus: dL + dR = 2 * mod_base_ms within 0.0001 ms at every sample; no NaN | max \|dL + dR - 2 base\| 0.000 ns over 3 settings; no NaN | pass |
| T8 | Micro-pitch 9 cents on sine(1000): pL peak 1005.21 Hz, pR 994.81 Hz, each within 0.58 Hz | pL 1005.21 Hz, pR 994.82 Hz | pass |
| T9 | Velvet on noise: \|corr(vL, vR)\| <= 0.25; RMS within 1 dB of input; third-octave levels 125 Hz - 16 kHz within 5 dB | corr 0.149; RMS L -0.00 dB, R 0.01 dB; worst third-octave deviation 4.44 dB | pass |
| T10 | Full analysis on mix: tonal + transient + noise - input delayed by Lat <= -90 dB | -139.0 dB | pass |
| T11 | ambience 0. sine(440): tonal within 1 dB, others <= -20 dB. clicks: transient >= 10 dB above others. noise: noise >= 3 dB above others | sine T/N/X -0.0/-48.5/-57.2 dB; clicks -600.0/-600.0/-0.0 dB; noise -11.4/-4.9/-11.6 dB | pass |
| T12 | decayTone, ambience 100 %, decay 1 s: magnitude-weighted mean of ma <= 0.1 over 0.3-1.0 s and >= 0.3 over 1.1-1.8 s | steady 0.002, decaying 0.686 | pass |
| T13 | twoSource, Groups, 1.5-2.5 s: exactly 2 groups; >= 12 of 16 partials in the right group; opposite pans; each source >= 6 dB louder on its own side | groups 2..2 over 94 frames; 15/16 partials correct; pans A -0.50, B 1.00; L-R A -6.1 dB, B 15.0 dB | pass |
| T14 | melody, same setup as T13: all four notes receive the same pan | pans -0.50, -0.50, -0.50, -0.50 | pass |
| T15 | toneClick, Spread only, duck 100 % vs 0: side energy -1..+5 ms around the click >= 20 dB lower (Full); +0.5..+5 ms >= 10 dB lower (Light) | Full 33.0 dB lower, Light 37.5 dB lower | pass |
| T16 | All generators at 100 %, width 200 %, guard On, noise: correlation of every 100 ms window after 500 ms >= -0.1 | minimum -0.071 | pass |
| T17 | Preset 16 with vs without forceAwake. gapNoise: difference <= -100 dB over the whole render. noise with each of Spread, Delay, Mod, Velvet at 0 for 1 s and back: <= -80 dB from 150 ms after each wake | part 1 -inf dB; part 2 Spread/Delay/Mod/Velvet -inf dB, -inf dB, -inf dB, -inf dB | pass |
| T18 | Preset 16, 20 s renders, median of 5: silent input <= 0.2 x the time of noise; all amounts 0 with noise <= 0.2 x | noise 0.563 s; silent 0.010 s (0.018x); all amounts 0 0.011 s (0.019x) | pass |
| T19 | Allocations inside processBlock, presets 1, 14, 16 with parameter automation: count is 0 | 0 allocations | pass |
| T20 | forceAwake, preset 16, noise 1 s then silence 5 s: no output sample with 0 < \|x\| < 1e-30 | 0 samples; smallest non-zero magnitude -600 dBFS | pass |
| T27 | Bypass on, stereo noise: output - input delayed by Lat <= -120 dB, both engines | Light -149.5 dB (Lat 0), Full -149.5 dB (Lat 2048) | pass |
