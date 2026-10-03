#include <catch2/catch_test_macros.hpp>

#include "common/MeasurePlugin.h"
#include "ui/Strings.h"

#include <juce_gui_basics/juce_gui_basics.h>

TEST_CASE ("T28 interface snapshot", "[T28][ui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    sph::ui::setPreferencesPersistent (false);
    const auto r = sph::measure::t28InterfaceSnapshot();
    INFO (r.measured);
    CHECK (r.pass);
}

TEST_CASE ("P2-T11 view snapshots", "[P2-T11][ui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    sph::ui::setPreferencesPersistent (false);
    const auto r = sph::measure::p2t11ViewSnapshots();
    INFO (r.measured);
    CHECK (r.pass);
}
