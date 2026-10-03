# Preset metrics

Written by `sph_measure --metrics`. Each factory preset on 6 s of `mix` (mono input, 48 kHz), measured after 1 s. These are objective companions to listening, not pass criteria (PART2_LEDGER.md, I12).

- **Correlation**: broadband L/R correlation.
- **ASW**: apparent source width from the virtual listener (0 = point source, 1 = fully diffuse).
- **Mono fold**: worst third-octave deviation of (L + R) / 2 from the input; 0 dB is mono-safe.
- **Loudness**: integrated loudness change, BS.1770, output against a dual-mono input.

| # | Preset | Correlation | ASW | Mono fold | Loudness |
| --- | --- | --- | --- | --- | --- |
| 1 | Default: Orban comb | 0.67 | 0.40 | 0.0 dB | 0.8 LU |
| 2 | Classic: Haas | -0.04 | 0.48 | -10.2 dB | -0.0 LU |
| 3 | Classic: Haas, mono-safe | 0.56 | 0.34 | 0.0 dB | 1.1 LU |
| 4 | Classic: Lauridsen | 0.28 | 0.32 | 0.0 dB | 2.0 LU |
| 5 | Classic: Orban, full depth | 0.29 | 0.37 | 0.0 dB | 2.0 LU |
| 6 | Classic: Shaped spread | 0.71 | 0.31 | 0.0 dB | 0.8 LU |
| 7 | Classic: Spectral split | 0.44 | 0.12 | 0.0 dB | 1.5 LU |
| 8 | Dimension chorus | 0.56 | 0.42 | -4.5 dB | 1.5 LU |
| 9 | Dimension chorus, mono-safe | 0.51 | 0.52 | 0.0 dB | 1.2 LU |
| 10 | Micro-pitch doubler | 0.77 | 0.47 | -4.9 dB | 1.1 LU |
| 11 | Micro-pitch, mono-safe | 0.74 | 0.51 | 0.0 dB | 0.7 LU |
| 12 | Velvet diffuse | 0.63 | 0.30 | 0.0 dB | 0.9 LU |
| 13 | Velvet, true decorrelation | 0.82 | 0.19 | -18.1 dB | 3.3 LU |
| 14 | Scene: Adaptive | 0.81 | 0.33 | 0.0 dB | 0.4 LU |
| 15 | Scene: Vocal | 0.93 | 0.28 | 0.0 dB | 0.1 LU |
| 16 | Scene: Ensemble | 0.58 | 0.35 | 0.0 dB | 1.2 LU |
| 17 | Scene: Stable partials | 0.90 | 0.23 | 0.0 dB | 0.4 LU |
