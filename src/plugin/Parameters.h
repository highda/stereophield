#pragma once

#include "dsp/Params.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace sph
{
// Parameter layout and the per-block snapshot of it.
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
    X (velvet_design) X (latency_mode) X (transient_mode) X (pan_ownership) X (width_mode) X (asw_target) X (guard_ceiling_db) \
    X (ui_mode) X (easy_width) X (easy_character) X (easy_space) X (easy_focus) X (easy_adapt) X (easy_low_latency)

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

// Parameters the engine reads (Params); after them come the Easy mode
// parameters, which only the mapping reads.
inline constexpr int numCoreParameters = 88;
inline constexpr int numParametersV2 = 87;

// Position of a parameter in ids::all, at compile time.
constexpr int indexOf (const char* id)
{
    for (int i = 0; i < numParameters; ++i)
    {
        const char* a = ids::all[i];
        int k = 0;
        while (a[k] != 0 && a[k] == id[k])
            ++k;
        if (a[k] == id[k])
            return i;
    }
    return -1;
}
static_assert (indexOf (ids::ui_mode) == numCoreParameters);

// A float parameter that keeps its exact plain value. JUCE's float
// parameter stores convertFrom0to1 (normalised), so some plain values (0 dB on
// a -24 .. +12 dB range needs exactly 2/3) can never be restored exactly, and a
// reload or undo would move the sound by a float step. Hosts and JUCE still
// see ordinary normalised values; the DSP reads plain().
class ExactFloatParameter : public juce::AudioParameterFloat
{
public:
    using juce::AudioParameterFloat::AudioParameterFloat;

    float plain() const noexcept { return exact.load (std::memory_order_relaxed); }

    // Sets the plain value exactly and notifies the host and listeners.
    void setPlain (float x)
    {
        setValueNotifyingHost (convertTo0to1 (x));
        exact.store (x, std::memory_order_relaxed);
    }

private:
    float getValue() const override { return convertTo0to1 (exact.load (std::memory_order_relaxed)); }
    // Everything JUCE and hosts use goes through these two overrides; the
    // base class's own copy (read only by its non-virtual get()) is unused.
    void setValue (float normalised) override { exact.store (convertFrom0to1 (normalised), std::memory_order_relaxed); }

    std::atomic<float> exact { get() };
};

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Value a parameter takes when a session saved by an older version (state
// version 1 or 2), before the parameter existed, is loaded: the behaviour of
// that version. Returns false if the default applies.
bool legacyValue (const juce::String& id, int version, float& plainValue);

// Parameter values, looked up once; read() converts them to Params.
//
// Values come from the parameter objects themselves, which hold exactly what
// was set or restored. `legacy` reads the APVTS mirror atomics instead, as
// 1.0 did; a session saved by 1.0 is rendered that way so that it sounds
// bit-identical (the mirror can sit a float step away after a reload).
class ParamReader
{
public:
    explicit ParamReader (juce::AudioProcessorValueTreeState& state);
    Params read (bool legacy = false) const noexcept;

    // Every parameter's plain value, in ids::all order.
    void values (float* out, bool legacy = false) const noexcept;

    // Params from plain values in ids::all order (the core parameters).
    static Params toParams (const float* values) noexcept;

private:
    float value (int i, bool legacy) const noexcept;

    std::atomic<float>* raw[numParameters] {};
    ExactFloatParameter* floats[numParameters] {};
    juce::AudioParameterChoice* choices[numParameters] {};
    juce::AudioParameterInt* ints[numParameters] {};
    juce::AudioParameterBool* bools[numParameters] {};
};
} // namespace sph
