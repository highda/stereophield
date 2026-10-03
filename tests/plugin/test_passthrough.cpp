#include <catch2/catch_test_macros.hpp>

#include "plugin/PluginProcessor.h"

TEST_CASE ("Pass-through copies mono input to both outputs", "[plugin]")
{
    sph::StereophieldProcessor p;
    p.setPlayConfigDetails (1, 2, 48000.0, 64);
    p.prepareToPlay (48000.0, 64);

    juce::AudioBuffer<float> buf (2, 64);
    buf.clear();
    for (int i = 0; i < 64; ++i)
        buf.setSample (0, i, 0.01f * (float) i);
    juce::MidiBuffer midi;
    p.processBlock (buf, midi);

    for (int i = 0; i < 64; ++i)
        REQUIRE (buf.getSample (1, i) == buf.getSample (0, i));
}
