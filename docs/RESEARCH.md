# Research results

Research items of PART2_LEDGER.md section 7. R1 (supervised NMF) and R2 (a learned separation front end) were dropped at the owner's request on 2026-10-03.

## R3: auto-width to a perceived-width target

**Question.** Can the plugin hold a perceived width constant as the material changes?

**Implementation.** `width_mode` Auto runs the virtual listener (spherical head, ±30° loudspeakers, IACC_E3 octaves) on the plugin's own output every 100 ms on the audio thread, with prepared buffers and no allocation. It moves the width gain toward `asw_target`: the step is proportional to the error, with a 2 s time constant, at most 3 dB per second, within -24 to +12 dB. The correlation guard still applies afterwards.

**Go/no-go criterion.** With a target of 0.3 on the drum loop and on `mix`, ASW must be within ±0.05 of the target in 95 % of 100 ms windows after 3 s.

**Measured** (an independent listener on the output, 15 s renders, default preset):

| Controller | Drum loop | mix |
| --- | --- | --- |
| 2 s, 3 dB/s (shipped) | 67.5 % | 95.8 % |
| 1 s, 6 dB/s | 10.0 % | 96.7 % |
| 0.5 s, 10 dB/s | 10.0 % | 100.0 % |
| 0.25 s, 20 dB/s | 0.0 % | 96.7 % |

**Result: no-go on percussive material, go on full mixes.** The drum loop cannot pass with any width controller. Its windows dominated by the kick sit below the 150 Hz bass-mono and are mono whatever the width, while hat and snare windows are wide; faster control only chases the hits and oscillates. On `mix` the image is held as intended.

**Decision.** The ledger says a no-go item stays disabled. The owner asked specifically to keep this item, so it ships **off by default** and is labelled **Auto (experimental)** in the interface, with the slow 2 s controller, which does not pump. Whether it stays is part of the human listening gate (PART2_LEDGER.md section 11), on full mixes and on drums.
