#include "plugin/Parameters.h"

#include <cmath>

namespace sph
{
namespace
{
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using Float = juce::AudioParameterFloat;
using Choice = juce::AudioParameterChoice;
using Int = juce::AudioParameterInt;
using Bool = juce::AudioParameterBool;
using Attr = juce::AudioParameterFloatAttributes;

juce::ParameterID pid (const char* id) { return { id, 1 }; }

juce::String withDecimals (float v, int decimals) { return juce::String (v, decimals); }

std::unique_ptr<Float> percent (const char* id, const char* name, float maxPercent, float def)
{
    return std::make_unique<Float> (pid (id), name, juce::NormalisableRange<float> (0.0f, maxPercent), def,
                                    Attr().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return withDecimals (v, 0) + " %"; }));
}

// Frequency ranges are skewed so that the geometric mean sits at the centre.
std::unique_ptr<Float> hertz (const char* id, const char* name, float lo, float hi, float def)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (std::sqrt (lo * hi));
    return std::make_unique<Float> (pid (id), name, r, def,
                                    Attr().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
                                    {
                                        return v >= 1000.0f ? withDecimals (v / 1000.0f, 2) + " kHz"
                                                            : withDecimals (v, v < 10.0f ? 2 : 0) + " Hz";
                                    }));
}

std::unique_ptr<Float> plain (const char* id, const char* name, float lo, float hi, float def, const char* unit, int decimals)
{
    const juce::String u (unit);
    return std::make_unique<Float> (pid (id), name, juce::NormalisableRange<float> (lo, hi), def,
                                    Attr().withLabel (u).withStringFromValueFunction ([u, decimals] (float v, int)
                                    {
                                        return withDecimals (v, decimals) + (u.isEmpty() ? "" : " " + u);
                                    }));
}

std::unique_ptr<Choice> choice (const char* id, const char* name, juce::StringArray items, int def,
                                bool automatable = true)
{
    return std::make_unique<Choice> (pid (id), name, items, def,
                                     juce::AudioParameterChoiceAttributes().withAutomatable (automatable));
}

const juce::StringArray sources { "Full", "Tonal", "Noise", "Tonal+Noise" };
} // namespace

Layout createParameterLayout()
{
    Layout l;
    // Global, section 8.2. The engine changes latency, so it is not automatable.
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

    // Generators, section 8.3.
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
    return l;
}

ParamReader::ParamReader (juce::AudioProcessorValueTreeState& state)
{
    for (int i = 0; i < numParameters; ++i)
    {
        raw[i] = state.getRawParameterValue (ids::all[i]);
        jassert (raw[i] != nullptr);
    }
}

Params ParamReader::read() const noexcept
{
    int i = 0;
    auto next = [&] { return raw[i++]->load (std::memory_order_relaxed); };
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
    jassert (i == numParameters);
    return p;
}
} // namespace sph
