# Parameters

Written by `sph_measure --parameters`. IDs are what hosts store in sessions and automation; the version is the release that added the parameter (sessions from older versions get that version's behaviour). In Easy mode the macros set every engine parameter, so host automation of engine parameters has no effect there.

## Global

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `engine` | Engine | Light, Full | Light | no | 1.0 |
| `latency_mode` | Latency mode | Per engine, Always Full | Per engine | no | 2.0 |
| `bypass` | Bypass | Off, On | Off | yes | 1.0 |

## Analysis

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `ambience` | Ambience | 0 % to 100 % | 50 % | yes | 1.0 |
| `room_decay_s` | Room decay | 0.30 s to 3.00 s | 1.00 s | yes | 1.0 |
| `transient_mode` | Transient detector | Envelope, Spectral flux | Spectral flux | yes | 2.0 |

## Spread

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `spread_amount` | Spread amount | 0 % to 100 % | 60 % | yes | 1.0 |
| `spread_source` | Spread source | Full, Tonal, Noise, Tonal+Noise | Full | yes | 1.0 |
| `spread_type` | Spread type | Delay, Cascade | Cascade | yes | 1.0 |
| `spread_time_ms` | Spread time | 1.0 ms to 30.0 ms | 10.0 ms | yes | 1.0 |
| `spread_density` | Spread density | 2 to 24 | 8 | yes | 1.0 |
| `spread_f_lo` | Spread low | 40 Hz to 1.00 kHz | 100 Hz | yes | 1.0 |
| `spread_f_hi` | Spread high | 2.00 kHz to 18.00 kHz | 10.00 kHz | yes | 1.0 |
| `spread_skew` | Spread skew | -1.00 to 1.00 | 0.00 | yes | 1.0 |
| `spread_q` | Spread Q | 0.30 to 4.00 | 0.70 | yes | 1.0 |

## Delay

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `delay_amount` | Delay amount | 0 % to 100 % | 0 % | yes | 1.0 |
| `delay_source` | Delay source | Full, Tonal, Noise, Tonal+Noise | Full | yes | 1.0 |
| `delay_time_ms` | Delay time | 0.1 ms to 40.0 ms | 15.0 ms | yes | 1.0 |
| `delay_lp_hz` | Delay low-pass | 1.00 kHz to 20.00 kHz | 20.00 kHz | yes | 1.0 |
| `delay_side` | Delay side | Right, Left | Right | yes | 1.0 |

## Mod

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `mod_amount` | Mod amount | 0 % to 100 % | 0 % | yes | 1.0 |
| `mod_source` | Mod source | Full, Tonal, Noise, Tonal+Noise | Full | yes | 1.0 |
| `mod_type` | Mod type | Chorus, Micro-pitch | Chorus | yes | 1.0 |
| `mod_rate_hz` | Mod rate | 0.05 Hz to 5.00 Hz | 0.40 Hz | yes | 1.0 |
| `mod_depth_ms` | Mod depth | 0.00 ms to 5.00 ms | 1.50 ms | yes | 1.0 |
| `mod_base_ms` | Mod base | 3.0 ms to 20.0 ms | 8.0 ms | yes | 1.0 |
| `mod_cents` | Mod cents | 0.0 ct to 25.0 ct | 9.0 ct | yes | 1.0 |
| `mod_predelay_ms` | Mod pre-delay | 0.0 ms to 30.0 ms | 12.0 ms | yes | 1.0 |

## Velvet

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `velvet_amount` | Velvet amount | 0 % to 100 % | 0 % | yes | 1.0 |
| `velvet_source` | Velvet source | Full, Tonal, Noise, Tonal+Noise | Full | yes | 1.0 |
| `velvet_size_ms` | Velvet size | 10.0 ms to 80.0 ms | 30.0 ms | yes | 1.0 |
| `velvet_density` | Velvet density | 500 /s to 3000 /s | 1000 /s | yes | 1.0 |
| `velvet_variation` | Velvet variation | 0 to 15 | 0 | yes | 1.0 |
| `velvet_design` | Velvet design | Random, Optimised | Optimised | yes | 2.0 |

## Pan map

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `pan_amount` | Pan map amount | 0 % to 100 % | 0 % | yes | 1.0 |
| `pan_mode` | Pan mode | Static, Tracks, Groups | Groups | yes | 1.0 |
| `pan_depth` | Pan depth | 0 % to 100 % | 70 % | yes | 1.0 |
| `pan_density` | Pan density | 0.25 c/oct to 4.00 c/oct | 1.00 c/oct | yes | 1.0 |
| `pan_bass_center_hz` | Pan bass centre | 60 Hz to 300 Hz | 120 Hz | yes | 1.0 |
| `pan_max_groups` | Pan groups | 2 to 8 | 6 | yes | 1.0 |
| `pan_ownership` | Pan bin ownership | Hard, Soft | Hard | yes | 2.0 |

## Coherence

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `coh_amount` | Coherence amount | 0 % to 100 % | 0 % | yes | 2.0 |
| `coh_source` | Coherence source | Full, Tonal, Noise, Tonal+Noise | Full | yes | 2.0 |
| `coh_mode` | Coherence mode | Curve, Spaced pair, Coincident pair, Near-coincident | Spaced pair | yes | 2.0 |
| `coh_p63` | Coherence 63 Hz | -0.50 to 1.00 | 0.90 | yes | 2.0 |
| `coh_p250` | Coherence 250 Hz | -0.50 to 1.00 | 0.60 | yes | 2.0 |
| `coh_p1k` | Coherence 1 kHz | -0.50 to 1.00 | 0.30 | yes | 2.0 |
| `coh_p4k` | Coherence 4 kHz | -0.50 to 1.00 | 0.10 | yes | 2.0 |
| `coh_p16k` | Coherence 16 kHz | -0.50 to 1.00 | 0.00 | yes | 2.0 |
| `coh_spacing_cm` | Mic spacing | 2 cm to 200 cm | 40 cm | yes | 2.0 |
| `coh_angle_deg` | Mic angle | 0 deg to 180 deg | 110 deg | yes | 2.0 |
| `coh_pattern` | Mic pattern | Omni, Subcardioid, Cardioid, Supercardioid, Figure-8 | Cardioid | yes | 2.0 |
| `coh_transient` | Coherence transient protection | 0 % to 100 % | 70 % | yes | 2.0 |

## Double

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `dbl_amount` | Double amount | 0 % to 100 % | 0 % | yes | 2.0 |
| `dbl_source` | Double source | Full, Tonal, Noise, Tonal+Noise | Full | yes | 2.0 |
| `dbl_offset_ms` | Double offset | 5.0 ms to 40.0 ms | 18.0 ms | yes | 2.0 |
| `dbl_drift_ms` | Double drift | 0.0 ms to 10.0 ms | 3.0 ms | yes | 2.0 |
| `dbl_drift_rate` | Double drift rate | 0.05 Hz to 2.00 Hz | 0.30 Hz | yes | 2.0 |
| `dbl_pitch_cents` | Double pitch drift | 0.0 ct to 20.0 ct | 4.0 ct | yes | 2.0 |
| `dbl_level_db` | Double level drift | 0.0 dB to 3.0 dB | 0.7 dB | yes | 2.0 |
| `dbl_tone_db` | Double tone | -6.0 dB to 6.0 dB | -1.5 dB | yes | 2.0 |
| `dbl_seed` | Double seed | 0 to 15 | 0 | yes | 2.0 |

## Room

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `room_amount` | Room amount | 0 % to 100 % | 0 % | yes | 2.0 |
| `room_source` | Room source | Full, Tonal, Noise, Tonal+Noise | Full | yes | 2.0 |
| `room_size` | Room size | 2.0 m to 30.0 m | 8.0 m | yes | 2.0 |
| `room_distance` | Room distance | 0.5 m to 8.0 m | 2.0 m | yes | 2.0 |
| `room_absorb` | Room absorption | 0 % to 100 % | 40 % | yes | 2.0 |
| `room_order` | Room order | 1, 2 | 2 | yes | 2.0 |
| `room_damp_hz` | Room damping | 2.00 kHz to 20.00 kHz | 9.00 kHz | yes | 2.0 |

## Image

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `img_amount` | Image expansion | 0 % to 300 % | 100 % | yes | 2.0 |
| `img_diffuse` | Image diffuse | 0 % to 200 % | 100 % | yes | 2.0 |
| `img_center_hz` | Image centre | 20 Hz to 500 Hz | 120 Hz | yes | 2.0 |

## Side bus

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `bass_mono_hz` | Bass mono | 20 Hz to 500 Hz | 150 Hz | yes | 1.0 |
| `band_xover_lo` | Crossover low | 100 Hz to 1.00 kHz | 400 Hz | yes | 1.0 |
| `band_xover_hi` | Crossover high | 1.00 kHz to 10.00 kHz | 4.00 kHz | yes | 1.0 |
| `band_low` | Low width | 0 % to 200 % | 100 % | yes | 1.0 |
| `band_mid` | Mid width | 0 % to 200 % | 100 % | yes | 1.0 |
| `band_high` | High width | 0 % to 200 % | 100 % | yes | 1.0 |
| `transient_duck` | Transient duck | 0 % to 100 % | 50 % | yes | 1.0 |
| `guard` | Guard | Off, On | On | yes | 1.0 |
| `guard_ceiling_db` | Guard ceiling | -12.0 dB to 0.0 dB | 0.0 dB | yes | 3.0 |

## Output

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `width` | Width | 0 % to 200 % | 100 % | yes | 1.0 |
| `mid_blend` | Mid blend | 0 % to 100 % | 0 % | yes | 1.0 |
| `comp_mode` | Compensation | Mono-exact, Constant loudness | Mono-exact | yes | 1.0 |
| `out_gain_db` | Output gain | -24.0 dB to 12.0 dB | 0.0 dB | yes | 1.0 |
| `listen` | Listen | Stereo, Mono, Side | Stereo | yes | 1.0 |
| `width_mode` | Width mode | Manual, Auto | Manual | yes | 2.0 |
| `asw_target` | Auto-width target | 0.00 to 0.55 | 0.30 | yes | 2.0 |

## Easy

| ID | Name | Range | Default | Automatable | Since |
| --- | --- | --- | --- | --- | --- |
| `ui_mode` | Mode | Easy, Complete | Easy | no | 3.0 |
| `easy_width` | Easy width | 0 % to 100 % | 50 % | yes | 3.0 |
| `easy_character` | Easy character | 0 % to 100 % | 40 % | yes | 3.0 |
| `easy_space` | Easy space | 0 % to 100 % | 20 % | yes | 3.0 |
| `easy_focus` | Easy focus | 0 % to 100 % | 50 % | yes | 3.0 |
| `easy_adapt` | Easy adapt | Off, On | On | yes | 3.0 |
| `easy_low_latency` | Easy low latency | Off, On | Off | no | 3.0 |
