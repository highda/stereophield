# Measurements

Rewritten by `sph_measure --all`. Until that tool exists, values are entered by hand at each gate.

| Test | Criterion | Measured | Result |
| --- | --- | --- | --- |
| T1 | STFT identity, error <= -100 dB at 44.1, 48, 96 kHz | -138.8 dB; latency 2048, 2048, 4096 (= N) | pass |
| T24 | `auval -v aufx Stph Hgda` ends with `AU VALIDATION SUCCEEDED` | succeeded (phase 0, pass-through) | pass |
