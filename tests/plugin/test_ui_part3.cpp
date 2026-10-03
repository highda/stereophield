#include <catch2/catch_test_macros.hpp>

#include "common/MeasurePlugin.h"
#include "common/Render.h"
#include "plugin/Parameters.h"
#include "ui/PluginEditor.h"
#include "ui/Strings.h"

using namespace sph;
using namespace sph::test;

namespace
{
struct Editor
{
    juce::ScopedJuceInitialiser_GUI gui;
    Plugin pl;
    std::unique_ptr<juce::AudioProcessorEditor> owner;
    PluginEditor* ed = nullptr;
    explicit Editor (bool easyMode)
    {
        ui::setPreferencesPersistent (false);
        pl.processor().setUiLanguage (0);
        pl.set (ids::ui_mode, easyMode ? 0.0f : 1.0f);
        pl.prepare();
        owner.reset (pl.processor().createEditor());
        ed = dynamic_cast<PluginEditor*> (owner.get());
        ed->setZoom (1.0f);
        ed->setInfoVisible (true);
        ed->refreshForTest();
    }
};
} // namespace

TEST_CASE ("A new instance opens in Easy mode", "[P3][ui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    StereophieldProcessor fresh;
    CHECK (fresh.isEasyMode());
}

TEST_CASE ("P3-T7 the interface asks before collapsing to Easy mode", "[P3-T7][ui]")
{
    Editor e (false);
    CHECK_FALSE (e.ed->easyShowing());
    e.pl.set (ids::spread_amount, 37.0f);
    e.pl.processor().commitUndoPoint();
    const auto before = e.pl.processor().snapshot();

    // Cancel changes nothing.
    e.ed->pressModeButtonForTest();
    REQUIRE (e.ed->collapseQuestionShowing());
    CHECK_FALSE (e.pl.processor().isEasyMode());
    e.ed->answerCollapseForTest (false);
    CHECK_FALSE (e.ed->collapseQuestionShowing());
    CHECK (e.pl.processor().snapshot() == before);

    // OK collapses, as one undo point.
    e.ed->pressModeButtonForTest();
    e.ed->answerCollapseForTest (true);
    e.ed->refreshForTest();
    CHECK (e.pl.processor().isEasyMode());
    CHECK (e.ed->easyShowing());
    CHECK (e.pl.processor().undo());
    CHECK (e.pl.processor().snapshot() == before);
}

TEST_CASE ("P3-T6 expanding from the interface needs no question", "[P3-T6][ui]")
{
    Editor e (true);
    CHECK (e.ed->easyShowing());
    e.ed->pressModeButtonForTest();
    CHECK_FALSE (e.ed->collapseQuestionShowing());
    e.ed->refreshForTest();
    CHECK_FALSE (e.pl.processor().isEasyMode());
    CHECK_FALSE (e.ed->easyShowing());
}

TEST_CASE ("P3-T11 Easy mode snapshots", "[P3-T11][ui]")
{
    const auto r = sph::measure::p3t11EasySnapshots();
    INFO (r.measured);
    CHECK (r.pass);
}

TEST_CASE ("Recalling the current program leaves an untouched instance in Easy mode", "[P3][ui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    StereophieldProcessor fresh;
    fresh.setCurrentProgram (0);
    CHECK (fresh.isEasyMode());
    fresh.setCurrentProgram (3);
    CHECK_FALSE (fresh.isEasyMode());
}
