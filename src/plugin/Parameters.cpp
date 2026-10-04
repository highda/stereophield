#include "plugin/Parameters.h"

#include <cmath>

namespace sph
{
namespace
{
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using Float = ExactFloatParameter;
using Choice = juce::AudioParameterChoice;
using Int = juce::AudioParameterInt;
using Bool = juce::AudioParameterBool;
using Attr = juce::AudioParameterFloatAttributes;

juce::ParameterID pid (const char* id, int version = 1) { return { id, version }; }

// juce::String (v, 0) prints every digit; 0 decimals means rounded here.
juce::String withDecimals (float v, int decimals)
{
    return decimals > 0 ? juce::String (v, decimals) : juce::String (juce::roundToInt (v));
}

std::unique_ptr<Float> percent (const char* id, const char* name, float maxPercent, float def, int v = 1)
{
    return std::make_unique<Float> (pid (id, v), name, juce::NormalisableRange<float> (0.0f, maxPercent), def,
                                    Attr().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return withDecimals (v, 0) + " %"; }));
}

// Frequency ranges are skewed so that the geometric mean sits at the centre.
std::unique_ptr<Float> hertz (const char* id, const char* name, float lo, float hi, float def, int v = 1)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (std::sqrt (lo * hi));
    return std::make_unique<Float> (pid (id, v), name, r, def,
                                    Attr().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
                                    {
                                        return v >= 1000.0f ? withDecimals (v / 1000.0f, 2) + " kHz"
                                                            : withDecimals (v, v < 10.0f ? 2 : 0) + " Hz";
                                    }));
}

std::unique_ptr<Float> plain (const char* id, const char* name, float lo, float hi, float def, const char* unit, int decimals,
                              int v = 1)
{
    const juce::String u (unit);
    return std::make_unique<Float> (pid (id, v), name, juce::NormalisableRange<float> (lo, hi), def,
                                    Attr().withLabel (u).withStringFromValueFunction ([u, decimals] (float v, int)
                                    {
                                        return withDecimals (v, decimals) + (u.isEmpty() ? "" : " " + u);
                                    }));
}

std::unique_ptr<Choice> choice (const char* id, const char* name, juce::StringArray items, int def,
                                bool automatable = true, int v = 1)
{
    return std::make_unique<Choice> (pid (id, v), name, items, def,
                                     juce::AudioParameterChoiceAttributes().withAutomatable (automatable));
}

const juce::StringArray sources { "Full", "Tonal", "Noise", "Tonal+Noise" };

struct ParameterList
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>>& v;
    template <typename P> void add (std::unique_ptr<P> p) { v.push_back (std::move (p)); }
};
} // namespace

Layout createParameterLayout()
{
    // Parameters are created in their 1.0 order and then placed in host
    // groups. Audio Unit hosts identify parameters by
    // a hash of their ID, so grouping changes no automation.
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> all;
    ParameterList l { all };
    // Global. The engine changes latency, so it is not automatable.
    l.add (choice (ids::engine, "Engine", { "Light", "Full" }, 0, false));
    l.add (percent (ids::width, "Width", 200.0f, 100.0f));
    l.add (percent (ids::mid_blend, "Mid blend", 100.0f, 0.0f));
    l.add (hertz (ids::bass_mono_hz, "Bass mono", 20.0f, 500.0f, 150.0f));
    l.add (hertz (ids::band_xover_lo, "Crossover low", 100.0f, 1000.0f, 400.0f));
    l.add (hertz (ids::band_xover_hi, "Crossover high", 1000.0f, 10000.0f, 4000.0f));
    l.add (percent (ids::band_low, "Low width", 200.0f, 100.0f));
    l.add (percent (ids::band_mid, "Mid width", 200.0f, 100.0f));
    l.add (percent (ids::band_high, "High width", 200.0f, 100.0f));
    l.add (percent (ids::transient_duck, "Transient duck", 100.0f, 50.0f));
    l.add (choice (ids::guard, "Guard", { "Off", "On" }, 1));
    l.add (choice (ids::comp_mode, "Compensation", { "Mono-exact", "Constant loudness" }, 0));
    l.add (plain (ids::out_gain_db, "Output gain", -24.0f, 12.0f, 0.0f, "dB", 1));
    l.add (choice (ids::listen, "Listen", { "Stereo", "Mono", "Side" }, 0));
    l.add (std::make_unique<Bool> (pid (ids::bypass), "Bypass", false));
    l.add (percent (ids::ambience, "Ambience", 100.0f, 50.0f));
    l.add (plain (ids::room_decay_s, "Room decay", 0.3f, 3.0f, 1.0f, "s", 2));

    // Generators.
    l.add (percent (ids::spread_amount, "Spread amount", 100.0f, 60.0f));
    l.add (choice (ids::spread_source, "Spread source", sources, 0));
    l.add (choice (ids::spread_type, "Spread type", { "Delay", "Cascade" }, 1));
    l.add (plain (ids::spread_time_ms, "Spread time", 1.0f, 30.0f, 10.0f, "ms", 1));
    l.add (std::make_unique<Int> (pid (ids::spread_density), "Spread density", 2, 24, 8));
    l.add (hertz (ids::spread_f_lo, "Spread low", 40.0f, 1000.0f, 100.0f));
    l.add (hertz (ids::spread_f_hi, "Spread high", 2000.0f, 18000.0f, 10000.0f));
    l.add (plain (ids::spread_skew, "Spread skew", -1.0f, 1.0f, 0.0f, "", 2));
    l.add (plain (ids::spread_q, "Spread Q", 0.3f, 4.0f, 0.7f, "", 2));

    l.add (percent (ids::delay_amount, "Delay amount", 100.0f, 0.0f));
    l.add (choice (ids::delay_source, "Delay source", sources, 0));
    l.add (plain (ids::delay_time_ms, "Delay time", 0.1f, 40.0f, 15.0f, "ms", 1));
    l.add (hertz (ids::delay_lp_hz, "Delay low-pass", 1000.0f, 20000.0f, 20000.0f));
    l.add (choice (ids::delay_side, "Delay side", { "Right", "Left" }, 0));

    l.add (percent (ids::mod_amount, "Mod amount", 100.0f, 0.0f));
    l.add (choice (ids::mod_source, "Mod source", sources, 0));
    l.add (choice (ids::mod_type, "Mod type", { "Chorus", "Micro-pitch" }, 0));
    l.add (hertz (ids::mod_rate_hz, "Mod rate", 0.05f, 5.0f, 0.4f));
    l.add (plain (ids::mod_depth_ms, "Mod depth", 0.0f, 5.0f, 1.5f, "ms", 2));
    l.add (plain (ids::mod_base_ms, "Mod base", 3.0f, 20.0f, 8.0f, "ms", 1));
    l.add (plain (ids::mod_cents, "Mod cents", 0.0f, 25.0f, 9.0f, "ct", 1));
    l.add (plain (ids::mod_predelay_ms, "Mod pre-delay", 0.0f, 30.0f, 12.0f, "ms", 1));

    l.add (percent (ids::velvet_amount, "Velvet amount", 100.0f, 0.0f));
    l.add (choice (ids::velvet_source, "Velvet source", sources, 0));
    l.add (plain (ids::velvet_size_ms, "Velvet size", 10.0f, 80.0f, 30.0f, "ms", 1));
    l.add (plain (ids::velvet_density, "Velvet density", 500.0f, 3000.0f, 1000.0f, "/s", 0));
    l.add (std::make_unique<Int> (pid (ids::velvet_variation), "Velvet variation", 0, 15, 0));

    l.add (percent (ids::pan_amount, "Pan map amount", 100.0f, 0.0f));
    l.add (choice (ids::pan_mode, "Pan mode", { "Static", "Tracks", "Groups" }, 2));
    l.add (percent (ids::pan_depth, "Pan depth", 100.0f, 70.0f));
    l.add (plain (ids::pan_density, "Pan density", 0.25f, 4.0f, 1.0f, "c/oct", 2));
    l.add (hertz (ids::pan_bass_center_hz, "Pan bass centre", 60.0f, 300.0f, 120.0f));
    l.add (std::make_unique<Int> (pid (ids::pan_max_groups), "Pan groups", 2, 8, 6));

    // Added in 2.0, version hint 2.
    constexpr int v2 = 2;
    l.add (percent (ids::coh_amount, "Coherence amount", 100.0f, 0.0f, v2));
    l.add (choice (ids::coh_source, "Coherence source", sources, 0, true, v2));
    l.add (choice (ids::coh_mode, "Coherence mode", { "Curve", "Spaced pair", "Coincident pair", "Near-coincident" }, 1, true, v2));
    l.add (plain (ids::coh_p63, "Coherence 63 Hz", -0.5f, 1.0f, 0.9f, "", 2, v2));
    l.add (plain (ids::coh_p250, "Coherence 250 Hz", -0.5f, 1.0f, 0.6f, "", 2, v2));
    l.add (plain (ids::coh_p1k, "Coherence 1 kHz", -0.5f, 1.0f, 0.3f, "", 2, v2));
    l.add (plain (ids::coh_p4k, "Coherence 4 kHz", -0.5f, 1.0f, 0.1f, "", 2, v2));
    l.add (plain (ids::coh_p16k, "Coherence 16 kHz", -0.5f, 1.0f, 0.0f, "", 2, v2));
    {
        juce::NormalisableRange<float> r (2.0f, 200.0f);
        r.setSkewForCentre (20.0f);
        l.add (std::make_unique<Float> (pid (ids::coh_spacing_cm, v2), "Mic spacing", r, 40.0f,
                                        Attr().withLabel ("cm").withStringFromValueFunction ([] (float v, int) { return withDecimals (v, 0) + " cm"; })));
    }
    l.add (plain (ids::coh_angle_deg, "Mic angle", 0.0f, 180.0f, 110.0f, "deg", 0, v2));
    l.add (choice (ids::coh_pattern, "Mic pattern", { "Omni", "Subcardioid", "Cardioid", "Supercardioid", "Figure-8" }, 2, true, v2));
    l.add (percent (ids::coh_transient, "Coherence transient protection", 100.0f, 70.0f, v2));

    l.add (percent (ids::dbl_amount, "Double amount", 100.0f, 0.0f, v2));
    l.add (choice (ids::dbl_source, "Double source", sources, 0, true, v2));
    l.add (plain (ids::dbl_offset_ms, "Double offset", 5.0f, 40.0f, 18.0f, "ms", 1, v2));
    l.add (plain (ids::dbl_drift_ms, "Double drift", 0.0f, 10.0f, 3.0f, "ms", 1, v2));
    l.add (hertz (ids::dbl_drift_rate, "Double drift rate", 0.05f, 2.0f, 0.3f, v2));
    l.add (plain (ids::dbl_pitch_cents, "Double pitch drift", 0.0f, 20.0f, 4.0f, "ct", 1, v2));
    l.add (plain (ids::dbl_level_db, "Double level drift", 0.0f, 3.0f, 0.7f, "dB", 1, v2));
    l.add (plain (ids::dbl_tone_db, "Double tone", -6.0f, 6.0f, -1.5f, "dB", 1, v2));
    l.add (std::make_unique<Int> (pid (ids::dbl_seed, v2), "Double seed", 0, 15, 0));

    l.add (percent (ids::room_amount, "Room amount", 100.0f, 0.0f, v2));
    l.add (choice (ids::room_source, "Room source", sources, 0, true, v2));
    {
        juce::NormalisableRange<float> r (2.0f, 30.0f);
        r.setSkewForCentre (std::sqrt (60.0f));
        l.add (std::make_unique<Float> (pid (ids::room_size, v2), "Room size", r, 8.0f,
                                        Attr().withLabel ("m").withStringFromValueFunction ([] (float v, int) { return withDecimals (v, 1) + " m"; })));
    }
    l.add (plain (ids::room_distance, "Room distance", 0.5f, 8.0f, 2.0f, "m", 1, v2));
    l.add (percent (ids::room_absorb, "Room absorption", 100.0f, 40.0f, v2));
    l.add (choice (ids::room_order, "Room order", { "1", "2" }, 1, true, v2));
    l.add (hertz (ids::room_damp_hz, "Room damping", 2000.0f, 20000.0f, 9000.0f, v2));

    l.add (percent (ids::img_amount, "Image expansion", 300.0f, 100.0f, v2));
    l.add (percent (ids::img_diffuse, "Image diffuse", 200.0f, 100.0f, v2));
    l.add (hertz (ids::img_center_hz, "Image centre", 20.0f, 500.0f, 120.0f, v2));

    l.add (choice (ids::velvet_design, "Velvet design", { "Random", "Optimised" }, 1, true, v2));
    l.add (choice (ids::latency_mode, "Latency mode", { "Per engine", "Always Full" }, 0, false, v2));
    l.add (choice (ids::transient_mode, "Transient detector", { "Envelope", "Spectral flux" }, 1, true, v2));
    l.add (choice (ids::pan_ownership, "Pan bin ownership", { "Hard", "Soft" }, 0, true, v2));
    l.add (choice (ids::width_mode, "Width mode", { "Manual", "Auto" }, 0, true, v2));
    l.add (plain (ids::asw_target, "Auto-width target", 0.0f, 0.55f, 0.3f, "", 2, v2));

    // Added in 3.0, version hint 3: the guard's ceiling, then Easy mode
    //.
    constexpr int v3 = 3;
    l.add (plain (ids::guard_ceiling_db, "Guard ceiling", -12.0f, 0.0f, 0.0f, "dB", 1, v3));
    l.add (choice (ids::ui_mode, "Mode", { "Easy", "Complete" }, 0, false, v3));
    l.add (percent (ids::easy_width, "Easy width", 100.0f, 50.0f, v3));
    l.add (percent (ids::easy_character, "Easy character", 100.0f, 40.0f, v3));
    l.add (percent (ids::easy_space, "Easy space", 100.0f, 20.0f, v3));
    l.add (percent (ids::easy_focus, "Easy focus", 100.0f, 50.0f, v3));
    l.add (choice (ids::easy_adapt, "Easy adapt", { "Off", "On" }, 1, true, v3));
    l.add (choice (ids::easy_low_latency, "Easy low latency", { "Off", "On" }, 0, false, v3));

    static const std::vector<std::pair<const char*, std::vector<const char*>>> groups {
        { "Global", { ids::engine, ids::latency_mode, ids::bypass } },
        { "Analysis", { ids::ambience, ids::room_decay_s, ids::transient_mode } },
        { "Spread", { ids::spread_amount, ids::spread_source, ids::spread_type, ids::spread_time_ms, ids::spread_density,
                      ids::spread_f_lo, ids::spread_f_hi, ids::spread_skew, ids::spread_q } },
        { "Delay", { ids::delay_amount, ids::delay_source, ids::delay_time_ms, ids::delay_lp_hz, ids::delay_side } },
        { "Mod", { ids::mod_amount, ids::mod_source, ids::mod_type, ids::mod_rate_hz, ids::mod_depth_ms, ids::mod_base_ms,
                   ids::mod_cents, ids::mod_predelay_ms } },
        { "Velvet", { ids::velvet_amount, ids::velvet_source, ids::velvet_size_ms, ids::velvet_density, ids::velvet_variation, ids::velvet_design } },
        { "Pan map", { ids::pan_amount, ids::pan_mode, ids::pan_depth, ids::pan_density, ids::pan_bass_center_hz, ids::pan_max_groups, ids::pan_ownership } },
        { "Coherence", { ids::coh_amount, ids::coh_source, ids::coh_mode, ids::coh_p63, ids::coh_p250, ids::coh_p1k, ids::coh_p4k,
                         ids::coh_p16k, ids::coh_spacing_cm, ids::coh_angle_deg, ids::coh_pattern, ids::coh_transient } },
        { "Double", { ids::dbl_amount, ids::dbl_source, ids::dbl_offset_ms, ids::dbl_drift_ms, ids::dbl_drift_rate, ids::dbl_pitch_cents,
                      ids::dbl_level_db, ids::dbl_tone_db, ids::dbl_seed } },
        { "Room", { ids::room_amount, ids::room_source, ids::room_size, ids::room_distance, ids::room_absorb, ids::room_order, ids::room_damp_hz } },
        { "Image", { ids::img_amount, ids::img_diffuse, ids::img_center_hz } },
        { "Side bus", { ids::bass_mono_hz, ids::band_xover_lo, ids::band_xover_hi, ids::band_low, ids::band_mid, ids::band_high,
                        ids::transient_duck, ids::guard, ids::guard_ceiling_db } },
        { "Output", { ids::width, ids::mid_blend, ids::comp_mode, ids::out_gain_db, ids::listen, ids::width_mode, ids::asw_target } },
        { "Easy", { ids::ui_mode, ids::easy_width, ids::easy_character, ids::easy_space, ids::easy_focus, ids::easy_adapt,
                    ids::easy_low_latency } },
    };
    Layout layout;
    for (const auto& [name, members] : groups)
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (juce::String (name).toLowerCase().removeCharacters (" "), name, " | ");
        for (const char* id : members)
            for (auto& p : all)
                if (p != nullptr && p->getParameterID() == id)
                    group->addChild (std::move (p));
        layout.add (std::move (group));
    }
    // Every parameter is in a group.
    jassert (std::all_of (all.begin(), all.end(), [] (const auto& p) { return p == nullptr; }));
    return layout;
}


bool legacyValue (const juce::String& id, int version, float& plainValue)
{
    // 1.0.0 behaviour for the algorithm choices added in 2.0.
    if (version < 2
        && (id == ids::velvet_design || id == ids::latency_mode || id == ids::transient_mode || id == ids::pan_ownership))
    {
        plainValue = 0.0f;
        return true;
    }
    // Sessions from before Easy mode open in the complete interface.
    if (version < 3 && id == ids::ui_mode)
    {
        plainValue = 1.0f;
        return true;
    }
    return false;
}

ParamReader::ParamReader (juce::AudioProcessorValueTreeState& state)
{
    for (int i = 0; i < numParameters; ++i)
    {
        raw[i] = state.getRawParameterValue (ids::all[i]);
        jassert (raw[i] != nullptr);
        auto* p = state.getParameter (ids::all[i]);
        floats[i] = dynamic_cast<ExactFloatParameter*> (p);
        choices[i] = dynamic_cast<juce::AudioParameterChoice*> (p);
        ints[i] = dynamic_cast<juce::AudioParameterInt*> (p);
        bools[i] = dynamic_cast<juce::AudioParameterBool*> (p);
    }
}

float ParamReader::value (int i, bool legacy) const noexcept
{
    if (legacy)
        return raw[i]->load (std::memory_order_relaxed);
    if (floats[i] != nullptr)
        return floats[i]->plain();
    if (choices[i] != nullptr)
        return (float) choices[i]->getIndex();
    if (ints[i] != nullptr)
        return (float) ints[i]->get();
    if (bools[i] != nullptr)
        return bools[i]->get() ? 1.0f : 0.0f;
    return raw[i]->load (std::memory_order_relaxed);
}

void ParamReader::values (float* out, bool legacy) const noexcept
{
    for (int i = 0; i < numParameters; ++i)
        out[i] = value (i, legacy);
}

Params ParamReader::read (bool legacy) const noexcept
{
    float v[numParameters];
    values (v, legacy);
    return toParams (v);
}

Params ParamReader::toParams (const float* v) noexcept
{
    int i = 0;
    auto next = [&] { return v[i++]; };
    auto pct = [&] { return next() * 0.01f; };
    auto idx = [&] { return (int) std::lround (next()); };

    Params p;
    p.engine = (Engine) idx();
    p.width = pct();
    p.midBlend = pct();
    p.bassMonoHz = next();
    p.bandXoverLo = next();
    p.bandXoverHi = next();
    p.bandLow = pct();
    p.bandMid = pct();
    p.bandHigh = pct();
    p.transientDuck = pct();
    p.guard = idx() != 0;
    p.compMode = (CompMode) idx();
    p.outGainDb = next();
    p.listen = (Listen) idx();
    p.bypass = next() >= 0.5f;
    p.ambience = pct();
    p.roomDecayS = next();

    p.spreadAmount = pct();
    p.spreadSource = (Source) idx();
    p.spreadType = (SpreadType) idx();
    p.spreadTimeMs = next();
    p.spreadDensity = idx();
    p.spreadFLo = next();
    p.spreadFHi = next();
    p.spreadSkew = next();
    p.spreadQ = next();

    p.delayAmount = pct();
    p.delaySource = (Source) idx();
    p.delayTimeMs = next();
    p.delayLpHz = next();
    p.delaySide = (HaasSide) idx();

    p.modAmount = pct();
    p.modSource = (Source) idx();
    p.modType = (ModType) idx();
    p.modRateHz = next();
    p.modDepthMs = next();
    p.modBaseMs = next();
    p.modCents = next();
    p.modPredelayMs = next();

    p.velvetAmount = pct();
    p.velvetSource = (Source) idx();
    p.velvetSizeMs = next();
    p.velvetDensity = next();
    p.velvetVariation = idx();

    p.panAmount = pct();
    p.panMode = (PanMode) idx();
    p.panDepth = pct();
    p.panDensity = next();
    p.panBassCenterHz = next();
    p.panMaxGroups = idx();

    p.cohAmount = pct();
    p.cohSource = (Source) idx();
    p.cohMode = (CohMode) idx();
    for (auto& c : p.cohPoints)
        c = next();
    p.cohSpacingCm = next();
    p.cohAngleDeg = next();
    p.cohPattern = (MicPattern) idx();
    p.cohTransient = pct();

    p.dblAmount = pct();
    p.dblSource = (Source) idx();
    p.dblOffsetMs = next();
    p.dblDriftMs = next();
    p.dblDriftRate = next();
    p.dblPitchCents = next();
    p.dblLevelDb = next();
    p.dblToneDb = next();
    p.dblSeed = idx();

    p.roomAmount = pct();
    p.roomSource = (Source) idx();
    p.roomSize = next();
    p.roomDistance = next();
    p.roomAbsorb = pct();
    p.roomOrder = idx() + 1;
    p.roomDampHz = next();

    p.imgAmount = pct();
    p.imgDiffuse = pct();
    p.imgCenterHz = next();

    p.velvetDesign = (VelvetDesign) idx();
    p.latencyMode = (LatencyMode) idx();
    p.transientMode = (TransientMode) idx();
    p.panOwnership = (PanOwnership) idx();
    p.widthMode = (WidthMode) idx();
    p.aswTarget = next();
    p.guardCeilingDb = next();
    jassert (i == numCoreParameters);
    return p;
}
} // namespace sph
