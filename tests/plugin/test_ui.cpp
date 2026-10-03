#include <catch2/catch_test_macros.hpp>

#include "common/MeasurePlugin.h"

#include <juce_gui_basics/juce_gui_basics.h>

TEST_CASE ("T28 interface snapshot", "[T28][ui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto r = sph::measure::t28InterfaceSnapshot();
    INFO (r.measured);
    CHECK (r.pass);
}
