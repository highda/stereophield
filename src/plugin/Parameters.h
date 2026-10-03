#pragma once

#include "dsp/Params.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace sph
{
// Parameter layout of DESIGN.md section 8 and the per-block snapshot of it.
namespace ids
{
#define SPH_PARAM_IDS(X) \
    X (engine) X (width) X (mid_blend) X (bass_mono_hz) X (band_xover_lo) X (band_xover_hi) \
    X (band_low) X (band_mid) X (band_high) X (transient_duck) X (guard) X (comp_mode) \
    X (out_gain_db) X (listen) X (bypass) X (ambience) X (room_decay_s) \
    X (spread_amount) X (spread_source) X (spread_type) X (spread_time_ms) X (spread_density) \
    X (spread_f_lo) X (spread_f_hi) X (spread_skew) X (spread_q) \
    X (delay_amount) X (delay_source) X (delay_time_ms) X (delay_lp_hz) X (delay_side) \
    X (mod_amount) X (mod_source) X (mod_type) X (mod_rate_hz) X (mod_depth_ms) X (mod_base_ms) \
    X (mod_cents) X (mod_predelay_ms) \
    X (velvet_amount) X (velvet_source) X (velvet_size_ms) X (velvet_density) X (velvet_variation) \
    X (pan_amount) X (pan_mode) X (pan_depth) X (pan_density) X (pan_bass_center_hz) X (pan_max_groups) \
    X (coh_amount) X (coh_source) X (coh_mode) X (coh_p63) X (coh_p250) X (coh_p1k) X (coh_p4k) X (coh_p16k) \
    X (coh_spacing_cm) X (coh_angle_deg) X (coh_pattern) X (coh_transient) \
    X (dbl_amount) X (dbl_source) X (dbl_offset_ms) X (dbl_drift_ms) X (dbl_drift_rate) X (dbl_pitch_cents) \
    X (dbl_level_db) X (dbl_tone_db) X (dbl_seed) \
    X (room_amount) X (room_source) X (room_size) X (room_distance) X (room_absorb) X (room_order) X (room_damp_hz) \
    X (img_amount) X (img_diffuse) X (img_center_hz) \
    X (velvet_design) X (latency_mode) X (transient_mode) X (pan_ownership)

#define SPH_DECLARE_ID(name) inline constexpr const char* name = #name;
SPH_PARAM_IDS (SPH_DECLARE_ID)
#undef SPH_DECLARE_ID

inline constexpr const char* all[] = {
#define SPH_LIST_ID(name) #name,
    SPH_PARAM_IDS (SPH_LIST_ID)
#undef SPH_LIST_ID
};
} // namespace ids

inline constexpr int numParameters = (int) (sizeof (ids::all) / sizeof (ids::all[0]));

inline constexpr int numParametersV1 = 50;

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Value a parameter takes when a session saved before it existed is loaded:
// the behaviour of that older version. Returns false if the default applies.
bool legacyValue (const juce::String& id, float& plainValue);

// Raw parameter values, looked up once; read() converts them to Params.
class ParamReader
{
public:
    explicit ParamReader (juce::AudioProcessorValueTreeState& state);
    Params read() const noexcept;

private:
    std::atomic<float>* raw[numParameters] {};
};
} // namespace sph
