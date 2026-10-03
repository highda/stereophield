#include <catch2/catch_test_macros.hpp>

#include "common/Fixtures.h"

using namespace sph;
using namespace sph::test;

// P2-T30: sessions saved by 1.0.0 load and render bit-identically.
TEST_CASE ("P2-T30 sessions from 1.0.0 render bit-identically", "[P2-T30][compat]")
{
    const auto dir = juce::File (SPH_SOURCE_DIR).getChildFile ("tests/fixtures");
    juce::StringArray lines;
    lines.addLines (dir.getChildFile ("hashes.txt").loadFileAsString());
    lines.removeEmptyStrings();
    REQUIRE (lines.size() == 20);
    int same = 0;
    for (const auto& line : lines)
    {
        const auto name = line.upToFirstOccurrenceOf (" ", false, false);
        const auto expected = line.fromFirstOccurrenceOf (" ", false, false).trim();
        juce::MemoryBlock state;
        REQUIRE (dir.getChildFile (name + ".state").loadFileAsData (state));
        Plugin pl;
        pl.processor().setStateInformation (state.getData(), (int) state.getSize());
        pl.prepare();
        const auto got = hashRender (pl.render (signals::mix (pl.fs)));
        INFO (name);
        CHECK (got == expected.toStdString());
        same += got == expected.toStdString() ? 1 : 0;
    }
    UNSCOPED_INFO (same << "/20 bit-identical");
}

TEST_CASE ("Legacy values apply only to states from 1.0", "[compat]")
{
    Plugin fresh;
    CHECK (fresh.get (ids::velvet_design) == 1.0f);
    CHECK (fresh.get (ids::transient_mode) == 1.0f);
    juce::MemoryBlock state;
    fresh.processor().getStateInformation (state);
    Plugin reload;
    reload.processor().setStateInformation (state.getData(), (int) state.getSize());
    CHECK (reload.get (ids::velvet_design) == 1.0f);
    CHECK (reload.get (ids::pan_ownership) == 1.0f);
}
