#include <catch2/catch_test_macros.hpp>

#include "common/Render.h"

using namespace sph;

namespace
{
struct Spec
{
    const char* id;
    float lo, hi, def;
};

// DESIGN.md sections 8.2 and 8.3, in parameter units. Choices are given as
// 0 .. (items - 1) with the default index.
const Spec table[] = {
    { "engine", 0, 1, 0 },           { "width", 0, 200, 100 },          { "mid_blend", 0, 100, 0 },
    { "bass_mono_hz", 20, 500, 150 }, { "band_xover_lo", 100, 1000, 400 }, { "band_xover_hi", 1000, 10000, 4000 },
    { "band_low", 0, 200, 100 },     { "band_mid", 0, 200, 100 },       { "band_high", 0, 200, 100 },
    { "transient_duck", 0, 100, 50 }, { "guard", 0, 1, 1 },             { "comp_mode", 0, 1, 0 },
    { "out_gain_db", -24, 12, 0 },   { "listen", 0, 2, 0 },             { "bypass", 0, 1, 0 },
    { "ambience", 0, 100, 50 },      { "room_decay_s", 0.3f, 3.0f, 1.0f },
    { "spread_amount", 0, 100, 60 }, { "spread_source", 0, 3, 0 },      { "spread_type", 0, 1, 1 },
    { "spread_time_ms", 1, 30, 10 }, { "spread_density", 2, 24, 8 },    { "spread_f_lo", 40, 1000, 100 },
    { "spread_f_hi", 2000, 18000, 10000 }, { "spread_skew", -1, 1, 0 }, { "spread_q", 0.3f, 4.0f, 0.7f },
    { "delay_amount", 0, 100, 0 },   { "delay_source", 0, 3, 0 },       { "delay_time_ms", 0.1f, 40, 15 },
    { "delay_lp_hz", 1000, 20000, 20000 }, { "delay_side", 0, 1, 0 },
    { "mod_amount", 0, 100, 0 },     { "mod_source", 0, 3, 0 },         { "mod_type", 0, 1, 0 },
    { "mod_rate_hz", 0.05f, 5, 0.4f }, { "mod_depth_ms", 0, 5, 1.5f },  { "mod_base_ms", 3, 20, 8 },
    { "mod_cents", 0, 25, 9 },       { "mod_predelay_ms", 0, 30, 12 },
    { "velvet_amount", 0, 100, 0 },  { "velvet_source", 0, 3, 0 },      { "velvet_size_ms", 10, 80, 30 },
    { "velvet_density", 500, 3000, 1000 }, { "velvet_variation", 0, 15, 0 },
    { "pan_amount", 0, 100, 0 },     { "pan_mode", 0, 2, 2 },           { "pan_depth", 0, 100, 70 },
    { "pan_density", 0.25f, 4, 1 },  { "pan_bass_center_hz", 60, 300, 120 }, { "pan_max_groups", 2, 8, 6 },
};

bool near (float a, float b) { return std::abs (a - b) <= 1e-4f * std::max (1.0f, std::abs (b)); }
} // namespace

TEST_CASE ("All 50 parameters match section 8", "[parameters]")
{
    test::Plugin pl;
    auto& state = pl.processor().state();
    REQUIRE (pl.processor().getParameters().size() == 50);
    REQUIRE (std::size (table) == 50);
    for (const auto& s : table)
    {
        INFO (s.id);
        auto* p = state.getParameter (s.id);
        REQUIRE (p != nullptr);
        CHECK (p->getVersionHint() == 1);
        CHECK (near (p->convertFrom0to1 (0.0f), s.lo));
        CHECK (near (p->convertFrom0to1 (1.0f), s.hi));
        CHECK (near (p->convertFrom0to1 (p->getDefaultValue()), s.def));
        CHECK (p->isAutomatable() == (juce::String (s.id) != "engine"));
    }
}

TEST_CASE ("Frequency parameters put the geometric mean at the centre", "[parameters]")
{
    test::Plugin pl;
    for (const char* id : { "bass_mono_hz", "band_xover_lo", "band_xover_hi", "spread_f_lo", "spread_f_hi",
                            "delay_lp_hz", "mod_rate_hz", "pan_bass_center_hz" })
    {
        INFO (id);
        auto* p = pl.processor().state().getParameter (id);
        const float lo = p->convertFrom0to1 (0.0f), hi = p->convertFrom0to1 (1.0f);
        CHECK (near (p->convertFrom0to1 (0.5f), std::sqrt (lo * hi)));
    }
}

TEST_CASE ("17 factory presets are exposed as programs", "[presets]")
{
    test::Plugin pl;
    auto& proc = pl.processor();
    REQUIRE (proc.getNumPrograms() == 17);
    CHECK (proc.getProgramName (0) == "Default: Orban comb");
    CHECK (proc.getProgramName (16) == "Scene: Stable partials");

    // Preset 2 changes exactly its listed parameters from the defaults.
    proc.setCurrentProgram (1);
    CHECK (proc.getCurrentProgram() == 1);
    CHECK (pl.get (ids::spread_amount) == 0.0f);
    CHECK (pl.get (ids::delay_amount) == 100.0f);
    CHECK (pl.get (ids::mid_blend) == 100.0f);
    CHECK (near (pl.get (ids::bass_mono_hz), 20.0f));
    CHECK (pl.get (ids::guard) == 0.0f);
    CHECK (pl.get (ids::width) == 100.0f);

    // Loading another preset resets everything not listed.
    proc.setCurrentProgram (0);
    CHECK (pl.get (ids::delay_amount) == 0.0f);
    CHECK (pl.get (ids::mid_blend) == 0.0f);
    CHECK (pl.get (ids::guard) == 1.0f);
}

TEST_CASE ("Bypass is the host bypass parameter", "[bypass]")
{
    test::Plugin pl;
    REQUIRE (pl.processor().getBypassParameter() == pl.processor().state().getParameter (ids::bypass));
}

TEST_CASE ("Buses: mono or stereo in, stereo out only", "[buses]")
{
    test::Plugin pl;
    auto& p = pl.processor();
    using Set = juce::AudioChannelSet;
    auto layout = [] (Set in, Set out)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.outputBuses.add (out);
        return l;
    };
    CHECK (p.checkBusesLayoutSupported (layout (Set::mono(), Set::stereo())));
    CHECK (p.checkBusesLayoutSupported (layout (Set::stereo(), Set::stereo())));
    CHECK_FALSE (p.checkBusesLayoutSupported (layout (Set::mono(), Set::mono())));
    CHECK_FALSE (p.checkBusesLayoutSupported (layout (Set::stereo(), Set::mono())));
}
