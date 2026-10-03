#include <catch2/catch_test_macros.hpp>

#include "common/Fixtures.h"

using namespace sph;
using namespace sph::test;
using sph::signals::samples;

TEST_CASE ("P2-T31 undo and A/B restore exactly", "[P2-T31]")
{
    Plugin pl;
    pl.prepare();
    auto& p = pl.processor();
    const auto start = p.snapshot();
    Rng rng (31);
    for (int i = 0; i < 50; ++i)
    {
        const char* id = ids::all[(size_t) (rng.uniform() * numParameters) % (size_t) numParameters];
        if (juce::String (id) == ids::engine || juce::String (id) == ids::latency_mode)
            continue;
        pl.setNormalised (id, (float) rng.uniform());
        p.commitUndoPoint();
    }
    const auto edited = p.snapshot();
    REQUIRE (! (edited == start));
    while (p.undo())
        ;
    for (int i = 0; i < numParameters; ++i)
        if (p.snapshot()[(size_t) i] != start[(size_t) i])
            UNSCOPED_INFO (ids::all[i] << ": " << p.snapshot()[(size_t) i] << " vs " << start[(size_t) i]);
    CHECK (p.snapshot() == start);
    while (p.redo())
        ;
    CHECK (p.snapshot() == edited);

    // Undone state renders bit-identically to a fresh instance at the start state.
    while (p.undo())
        ;
    Plugin fresh;
    fresh.prepare();
    pl.prepare();
    const auto x = signals::mix (pl.fs);
    CHECK (hashRender (pl.render (x)) == hashRender (fresh.render (x)));

    // A/B: edit A, switch to B (a copy), change B, back to A.
    pl.set (ids::width, 150);
    p.copyCompareAToB();
    const auto a = p.snapshot();
    p.selectCompareSlot (1);
    pl.set (ids::width, 40);
    pl.set (ids::spread_q, 2.71f);
    const auto b = p.snapshot();
    p.selectCompareSlot (0);
    CHECK (p.snapshot() == a);
    p.selectCompareSlot (1);
    CHECK (p.snapshot() == b);
}

TEST_CASE ("P2-T32 user presets round-trip and reject bad files", "[P2-T32]")
{
    Plugin pl;
    auto& p = pl.processor();
    const juce::String name = "zz test preset (delete me)";
    pl.set (ids::width, 133);
    pl.set (ids::coh_spacing_cm, 37.3f);
    REQUIRE (p.saveUserPreset (name));
    REQUIRE (p.userPresets().contains (name));
    const auto saved = p.snapshot();
    pl.set (ids::width, 10);
    REQUIRE (p.loadUserPreset (name));
    CHECK (p.snapshot() == saved);

    // Renamed by saving under a new name and deleting the old one.
    REQUIRE (p.saveUserPreset (name + " 2"));
    REQUIRE (p.deleteUserPreset (name));
    CHECK (! p.userPresets().contains (name));
    CHECK (p.loadUserPreset (name + " 2"));
    CHECK (p.deleteUserPreset (name + " 2"));

    // Malformed and foreign files are rejected without touching the state.
    const auto folder = StereophieldProcessor::userPresetFolder();
    folder.getChildFile ("zz broken.sphpreset").replaceWithText ("<not xml");
    folder.getChildFile ("zz foreign.sphpreset").replaceWithText ("<OTHERPLUGIN value=\"1\"/>");
    const auto before = p.snapshot();
    CHECK (! p.loadUserPreset ("zz broken"));
    CHECK (! p.loadUserPreset ("zz foreign"));
    CHECK (p.snapshot() == before);
    folder.getChildFile ("zz broken.sphpreset").deleteFile();
    folder.getChildFile ("zz foreign.sphpreset").deleteFile();
}

TEST_CASE ("P2-T8 teaching sources", "[P2-T8]")
{
    for (int source = 1; source <= 5; ++source)
    {
        INFO ("source " << source);
        Plugin pl;
        pl.set (ids::bypass, 1);
        pl.prepare();
        pl.processor().setTeachingSource (source);
        const auto y = pl.render (Signal ((size_t) samples (2.0, pl.fs), 0.0f));
        const Signal expected = source == 1 ? signals::noise (samples (10.0, pl.fs))
                              : source == 2 ? signals::twoSource (pl.fs)
                              : source == 3 ? signals::melody (pl.fs)
                              : source == 4 ? signals::toneClick (pl.fs)
                                            : signals::drumLoop (pl.fs);
        // Bypass passes the source through unchanged (Light engine, no latency).
        bool same = true;
        for (size_t i = 0; i < y.l.size(); ++i)
            same = same && y.l[i] == expected[i % expected.size()] && y.r[i] == y.l[i];
        CHECK (same);
    }
    Plugin pl;
    pl.processor().setTeachingSource (3);
    juce::MemoryBlock state;
    pl.processor().getStateInformation (state);
    pl.processor().setStateInformation (state.getData(), (int) state.getSize());
    CHECK (pl.processor().teachingSource() == 0);
}
