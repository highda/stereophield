# Measurements

Written by `sph_measure --all` on 2026-10-03. Release build, 48 kHz and block 512 unless a criterion says otherwise.

| Test | Criterion | Measured | Result |
| --- | --- | --- | --- |
| T1 | STFT identity: output minus input delayed by the measured latency <= -100 dB at 44.1, 48, 96 kHz | worst -138.8 dB; latency 2048, 2048, 4096 samples (N) | pass |
| T2 | Mono-safe: (L+R)/2 - out_gain * M_d <= -120 dB (every preset with mid_blend 0, 20 random sets; mix; 44.1, 48, 96 kHz) | worst -131.9 dB over 111 renders | pass |
| T3 | Peak of (L+R)/2 for an impulse with width 0 is at getLatencySamples(); Light gives 0 | Light 0 0 0; Full 2048 2048 4096 samples at 44.1/48/96 kHz | pass |
| T4 | Spread: \|L\|^2 + \|R\|^2 flat within 0.1 dB from 20 Hz to 20 kHz, both types | Delay 0.0000 dB, Cascade 0.0000 dB | pass |
| T5 | Spread: Pearson correlation of \|L\|^2 and \|R\|^2 at 400 log points 50 Hz - 15 kHz <= -0.9, both types | Delay -1.0000, Cascade -1.0000 | pass |
| T6 | Preset 2 on noise: L - input <= -100 dB; R - input delayed 720 samples <= -60 dB | L -138.5 dB, R -137.9 dB | pass |
| T7 | Chorus: dL + dR = 2 * mod_base_ms within 0.0001 ms at every sample; no NaN | max \|dL + dR - 2 base\| 0.000 ns over 3 settings; no NaN | pass |
| T8 | Micro-pitch 9 cents on sine(1000): pL peak 1005.21 Hz, pR 994.81 Hz, each within 0.58 Hz | pL 1005.21 Hz, pR 994.82 Hz | pass |
| T9 | Velvet on noise: \|corr(vL, vR)\| <= 0.25; RMS within 1 dB of input; third-octave levels 125 Hz - 16 kHz within 5 dB | corr 0.149; RMS L -0.00 dB, R 0.01 dB; worst third-octave deviation 4.44 dB | pass |
| T16 | All generators at 100 %, width 200 %, guard On, noise: correlation of every 100 ms window after 500 ms >= -0.1 | minimum -0.044 | pass |
| T27 | Bypass on, stereo noise: output - input delayed by Lat <= -120 dB, both engines | Light -149.5 dB (Lat 0), Full -149.5 dB (Lat 2048) | pass |
