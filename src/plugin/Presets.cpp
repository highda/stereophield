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
    };
    return presets;
}

void applyPreset (juce::AudioProcessorValueTreeState& state, const Preset& preset)
{
    for (const char* id : ids::all)
        if (auto* p = state.getParameter (id))
            p->setValueNotifyingHost (p->getDefaultValue());
    for (const auto& [id, value] : preset.changes)
        if (auto* p = state.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}
} // namespace sph
