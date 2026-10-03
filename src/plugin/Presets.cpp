#include "plugin/Presets.h"

#include "plugin/Parameters.h"

namespace sph
{
const std::vector<Preset>& factoryPresets()
{
    // Choice indices: engine Light 0 / Full 1; guard Off 0 / On 1;
    // spread_type Delay 0 / Cascade 1; mod_type Chorus 0 / Micro-pitch 1;
    // sources Full 0 / Tonal 1 / Noise 2 / Tonal+Noise 3;
    // pan_mode Static 0 / Tracks 1 / Groups 2.
    static const std::vector<Preset> presets {
        { "Default: Orban comb", {} },
        { "Classic: Haas", { { ids::spread_amount, 0 }, { ids::delay_amount, 100 }, { ids::mid_blend, 100 },
                             { ids::bass_mono_hz, 20 }, { ids::transient_duck, 0 }, { ids::guard, 0 } } },
        { "Classic: Haas, mono-safe", { { ids::spread_amount, 0 }, { ids::delay_amount, 100 } } },
        { "Classic: Lauridsen", { { ids::spread_type, 0 }, { ids::spread_amount, 100 }, { ids::spread_time_ms, 10 } } },
        { "Classic: Orban, full depth", { { ids::spread_amount, 100 } } },
        { "Classic: Shaped spread", { { ids::spread_amount, 80 }, { ids::spread_density, 14 }, { ids::spread_f_lo, 200 },
                                      { ids::spread_f_hi, 12000 }, { ids::spread_skew, 0.5f }, { ids::spread_q, 1.0f },
                                      { ids::band_low, 50 }, { ids::band_high, 130 } } },
        { "Classic: Spectral split", { { ids::engine, 1 }, { ids::spread_amount, 0 }, { ids::pan_amount, 100 },
                                       { ids::pan_mode, 0 }, { ids::pan_depth, 100 }, { ids::pan_density, 0.5f },
                                       { ids::transient_duck, 0 } } },
        { "Dimension chorus", { { ids::spread_amount, 0 }, { ids::mod_amount, 100 }, { ids::mid_blend, 100 } } },
        { "Dimension chorus, mono-safe", { { ids::spread_amount, 0 }, { ids::mod_amount, 100 } } },
        { "Micro-pitch doubler", { { ids::spread_amount, 0 }, { ids::mod_amount, 100 }, { ids::mod_type, 1 },
                                   { ids::mid_blend, 100 } } },
        { "Micro-pitch, mono-safe", { { ids::spread_amount, 0 }, { ids::mod_amount, 100 }, { ids::mod_type, 1 } } },
        { "Velvet diffuse", { { ids::spread_amount, 0 }, { ids::velvet_amount, 100 } } },
        { "Velvet, true decorrelation", { { ids::spread_amount, 0 }, { ids::velvet_amount, 100 }, { ids::mid_blend, 100 } } },
        { "Scene: Adaptive", { { ids::engine, 1 }, { ids::spread_amount, 0 }, { ids::pan_amount, 100 },
                               { ids::velvet_amount, 60 }, { ids::velvet_source, 2 }, { ids::transient_duck, 70 } } },
        { "Scene: Vocal", { { ids::engine, 1 }, { ids::spread_amount, 0 }, { ids::mod_amount, 60 }, { ids::mod_type, 1 },
                            { ids::mod_source, 1 }, { ids::velvet_amount, 50 }, { ids::velvet_source, 2 },
                            { ids::transient_duck, 80 }, { ids::bass_mono_hz, 180 } } },
        { "Scene: Ensemble", { { ids::engine, 1 }, { ids::spread_amount, 30 }, { ids::spread_source, 2 },
                               { ids::pan_amount, 100 }, { ids::pan_depth, 90 }, { ids::pan_max_groups, 8 },
                               { ids::velvet_amount, 40 }, { ids::velvet_source, 2 } } },
        { "Scene: Stable partials", { { ids::engine, 1 }, { ids::spread_amount, 0 }, { ids::pan_amount, 100 },
                                      { ids::pan_mode, 1 } } },
        // Part 2 (PART2_LEDGER.md section 10).
        { "Virtual pair: AB omnis 40 cm", { { ids::engine, 1 }, { ids::spread_amount, 0 }, { ids::coh_amount, 100 },
                                            { ids::coh_mode, 1 }, { ids::coh_spacing_cm, 40 } } },
        { "Virtual pair: XY cardioids", { { ids::engine, 1 }, { ids::spread_amount, 0 }, { ids::coh_amount, 100 },
                                          { ids::coh_mode, 2 }, { ids::coh_pattern, 2 }, { ids::coh_angle_deg, 90 } } },
        { "Virtual pair: Blumlein", { { ids::engine, 1 }, { ids::spread_amount, 0 }, { ids::coh_amount, 100 },
                                      { ids::coh_mode, 2 }, { ids::coh_pattern, 4 }, { ids::coh_angle_deg, 90 } } },
        { "Virtual pair: ORTF", { { ids::engine, 1 }, { ids::spread_amount, 0 }, { ids::coh_amount, 100 },
                                  { ids::coh_mode, 3 }, { ids::coh_pattern, 2 }, { ids::coh_angle_deg, 110 },
                                  { ids::coh_spacing_cm, 17 } } },
        { "Coherence: tight lows, open highs", { { ids::engine, 1 }, { ids::spread_amount, 0 }, { ids::coh_amount, 100 },
                                                 { ids::coh_mode, 0 }, { ids::coh_p63, 1.0f }, { ids::coh_p250, 0.8f },
                                                 { ids::coh_p1k, 0.3f }, { ids::coh_p4k, 0.0f }, { ids::coh_p16k, -0.2f } } },
        { "Double track: vocal", { { ids::spread_amount, 0 }, { ids::dbl_amount, 100 }, { ids::dbl_offset_ms, 18 },
                                   { ids::dbl_drift_ms, 3 }, { ids::dbl_pitch_cents, 4 } } },
        { "Double track: guitar wall", { { ids::spread_amount, 0 }, { ids::dbl_amount, 100 }, { ids::dbl_offset_ms, 25 },
                                         { ids::dbl_pitch_cents, 7 }, { ids::mid_blend, 100 } } },
        { "Room cues: small studio", { { ids::spread_amount, 0 }, { ids::room_amount, 70 }, { ids::room_size, 6 },
                                       { ids::room_order, 1 } } },
        { "Room cues: wide hall edge", { { ids::spread_amount, 0 }, { ids::room_amount, 90 }, { ids::room_size, 20 },
                                         { ids::room_distance, 4 } } },
        { "Image expander: 150 %", { { ids::engine, 1 }, { ids::spread_amount, 0 }, { ids::img_amount, 150 } } },
        { "Scene: Natural ensemble", { { ids::engine, 1 }, { ids::spread_amount, 0 }, { ids::pan_amount, 100 },
                                       { ids::coh_amount, 50 }, { ids::coh_mode, 1 }, { ids::coh_spacing_cm, 60 },
                                       { ids::room_amount, 30 } } },
        { "Scene: Intimate vocal", { { ids::engine, 1 }, { ids::spread_amount, 0 }, { ids::dbl_amount, 60 },
                                     { ids::dbl_source, 1 }, { ids::coh_amount, 40 }, { ids::coh_source, 2 },
                                     { ids::transient_duck, 80 } } },
    };
    return presets;
}

void applyPreset (juce::AudioProcessorValueTreeState& state, const Preset& preset)
{
    // The core parameters only: the mode and the Easy macros stay.
    for (int i = 0; i < numCoreParameters; ++i)
        if (auto* p = state.getParameter (ids::all[i]))
            p->setValueNotifyingHost (p->getDefaultValue());
    for (const auto& [id, value] : preset.changes)
        if (auto* p = state.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}
} // namespace sph
