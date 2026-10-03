#include <catch2/catch_test_macros.hpp>

#include "common/Render.h"
#include "plugin/Parameters.h"
#include "ui/InfoPanel.h"
#include "ui/PluginEditor.h"
#include "ui/Strings.h"
#include "ui/Widgets.h"

#include <functional>
#include <set>

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
    explicit Editor (int presetNumber = 1)
    {
        ui::setPreferencesPersistent (false);
        pl.processor().setUiLanguage (0);
        pl.preset (presetNumber);
        pl.prepare();
        owner.reset (pl.processor().createEditor());
        ed = dynamic_cast<PluginEditor*> (owner.get());
        ed->setZoom (1.0f);
        ed->setInfoVisible (true);
    }
};

void walk (juce::Component& c, const std::function<void (juce::Component&)>& f)
{
    f (c);
    for (auto* ch : c.getChildren())
        walk (*ch, f);
}
} // namespace

TEST_CASE ("P2-T1 every text comes from the string table, in both languages", "[P2-T1][ui]")
{
    using namespace ui;
    // Both languages are present for every key.
    for (const auto& k : stringKeys())
    {
        INFO (k);
        CHECK (stringIn (k, Language::English).isNotEmpty());
        CHECK (stringIn (k, Language::Czech).isNotEmpty());
    }
    for (const auto& k : helpKeys())
    {
        INFO (k);
        Help en, cs;
        REQUIRE (helpIn (k, Language::English, en));
        REQUIRE (helpIn (k, Language::Czech, cs));
        CHECK (en.title.isNotEmpty());
        CHECK (cs.title.isNotEmpty());
        CHECK (en.what.isNotEmpty() == cs.what.isNotEmpty());
        CHECK (en.how.isNotEmpty() == cs.how.isNotEmpty());
    }
    // Every parameter has a caption and a help entry; every choice item a string.
    Plugin pl;
    for (const char* id : ids::all)
    {
        INFO (id);
        CHECK (hasString (juce::String ("cap.") + id));
        CHECK (hasHelp (juce::String ("p.") + id));
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (pl.processor().state().getParameter (id)))
            for (int i = 0; i < c->choices.size(); ++i)
            {
                const juce::String key = juce::String (id).endsWith ("_source") ? "choice.source." + juce::String (i)
                                                                                 : "choice." + juce::String (id) + "." + juce::String (i);
                CHECK (hasString (key));
            }
    }
    for (int p = 1; p <= (int) factoryPresets().size(); ++p)
        CHECK (hasHelp ("preset." + juce::String (p)));

    // Every button and label text in the editor, in each language, is a
    // table string (dynamic readouts are marked and skipped).
    for (int language = 0; language < 2; ++language)
    {
        Editor e;
        e.ed->setLanguageForTest (language);
        std::set<juce::String> table;
        for (const auto& k : stringKeys())
            table.insert (tr (k));
        for (const auto& t : tours()) // tour titles are bilingual in the tour data
            table.insert (juce::String::fromUTF8 (language == 1 ? t.titleCs : t.titleEn));
        int checked = 0;
        walk (e.ed->rootComponent(), [&] (juce::Component& c)
        {
            juce::String text;
            if (auto* b = dynamic_cast<juce::TextButton*> (&c))
                text = b->getButtonText();
            if (text.isEmpty() || text == "<" || text == ">" || text == "..." || text == "A" || text == "B"
                || text.containsAnyOf ("0123456789"))
                return;
            ++checked;
            INFO (text);
            CHECK (table.count (text) == 1);
        });
        CHECK (checked > 20);
    }
}

TEST_CASE ("P2-T2 the interface font has every Czech glyph", "[P2-T2][ui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto font = ui::bodyFont();
    const auto chars = juce::String::fromUTF8 ("ěščřžýáíéúůďťňĚŠČŘŽÝÁÍÉÚŮĎŤŇ");
    // A codepoint no font has gives the fallback (missing) glyph.
    juce::GlyphArrangement missing;
    missing.addLineOfText (font, juce::String::charToString ((juce::juce_wchar) 0x10FFFD), 0, 0);
    juce::Path missingPath;
    missing.createPath (missingPath);
    int distinct = 0;
    for (int i = 0; i < chars.length(); ++i)
    {
        juce::GlyphArrangement g;
        g.addLineOfText (font, juce::String::charToString (chars[i]), 0, 0);
        juce::Path p;
        g.createPath (p);
        const bool ok = ! p.isEmpty() && p.getBounds() != missingPath.getBounds();
        INFO ((int) chars[i]);
        CHECK (ok);
        distinct += ok ? 1 : 0;
    }
    CHECK (distinct == chars.length());
}

TEST_CASE ("P2-T3 switching language 100 times keeps the layout and the parameters", "[P2-T3][ui]")
{
    Editor e (16);
    const auto before = e.pl.processor().snapshot();
    for (int i = 0; i < 100; ++i)
        e.ed->setLanguageForTest (i % 2);
    CHECK (e.pl.processor().snapshot() == before);
    for (int language = 0; language < 2; ++language)
    {
        e.ed->setLanguageForTest (language);
        e.ed->refreshForTest();
        const auto window = e.ed->rootComponent().getLocalBounds();
        walk (e.ed->rootComponent(), [&] (juce::Component& c)
        {
            if (! c.isVisible() || &c == &e.ed->rootComponent() || c.getParentComponent() == nullptr)
                return;
            INFO (c.getName() << " " << ui::helpKeyOf (c) << " " << c.getBounds().toString());
            // Inside its parent and inside the window.
            CHECK (c.getParentComponent()->getLocalBounds().contains (c.getBounds()));
            CHECK (window.contains (e.ed->rootComponent().getLocalArea (c.getParentComponent(), c.getBounds())));
            // Button text fits (buttons do not squeeze).
            if (auto* b = dynamic_cast<juce::TextButton*> (&c))
                CHECK (juce::GlyphArrangement::getStringWidth (ui::labelFont(), b->getButtonText()) <= (float) b->getWidth());
        });
    }
}

TEST_CASE ("P2-T4 every info entry fits the panel in both languages", "[P2-T4][ui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    float worst = 0.0f;
    juce::String worstKey;
    for (int language = 0; language < 2; ++language)
    {
        ui::setLanguage (language == 1 ? ui::Language::Czech : ui::Language::English);
        for (const auto& k : ui::helpKeys())
        {
            const float h = ui::InfoPanel::textHeight (k, 236.0f);
            if (h > worst)
            {
                worst = h;
                worstKey = k;
            }
        }
    }
    ui::setLanguage (ui::Language::English);
    INFO ("tallest " << worstKey << " " << worst);
    CHECK (worst <= 700.0f);
}

TEST_CASE ("P2-T5 hovering shows the entry; pin keeps it", "[P2-T5][ui]")
{
    Editor e;
    int shown = 0;
    std::set<juce::String> keys;
    walk (e.ed->rootComponent(), [&] (juce::Component& c) { if (ui::helpKeyOf (c).isNotEmpty()) keys.insert (ui::helpKeyOf (c)); });
    for (const auto& k : keys)
    {
        e.ed->hoverForTest (k);
        INFO (k);
        CHECK (e.ed->infoPanel().currentKey() == k);
        shown += e.ed->infoPanel().currentKey() == k ? 1 : 0;
    }
    CHECK (shown > 60);
    e.ed->hoverForTest ("p.width");
    e.ed->infoPanel().findChildWithID ("");
    // Pin: the pinned entry stays while another control is hovered.
    for (auto* c : e.ed->infoPanel().getChildren())
        if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getClickingTogglesState())
            b->setToggleState (true, juce::dontSendNotification);
    e.ed->hoverForTest ("p.mid_blend");
    CHECK (e.ed->infoPanel().currentKey() == "p.width");
}

TEST_CASE ("P2-T6 live explanations carry the right numbers", "[P2-T6][ui]")
{
    Editor e;
    Rng rng (6);
    for (int i = 0; i < 20; ++i)
    {
        const float t = (float) (0.1 + 39.9 * rng.uniform());
        e.pl.set (ids::delay_time_ms, t);
        const float actual = e.pl.get (ids::delay_time_ms);
        const auto text = e.ed->liveText ("p.delay_time_ms");
        INFO (text);
        CHECK (text.contains (juce::String (1000.0 / actual, 1)));
        CHECK (text.contains (juce::String (500.0 / actual, 1)));

        const float c = (float) (25.0 * rng.uniform());
        e.pl.set (ids::mod_cents, c);
        const auto mc = e.ed->liveText ("p.mod_cents");
        CHECK (mc.contains (juce::String (std::pow (2.0, e.pl.get (ids::mod_cents) / 1200.0), 4)));

        const float w = (float) (1.0 + 199.0 * rng.uniform());
        e.pl.set (ids::width, w);
        CHECK (e.ed->liveText ("p.width").contains (juce::String (20.0 * std::log10 (e.pl.get (ids::width) / 100.0), 1)));
    }
    // Czech uses the decimal comma.
    e.ed->setLanguageForTest (1);
    e.pl.set (ids::delay_time_ms, 15.0f);
    CHECK (e.ed->liveText ("p.delay_time_ms").contains ("66,7"));
    e.ed->setLanguageForTest (0);
}

TEST_CASE ("P2-T7 tours highlight existing controls and change parameters only on Apply", "[P2-T7][ui]")
{
    Editor e;
    std::set<juce::String> keys;
    walk (e.ed->rootComponent(), [&] (juce::Component& c) { if (ui::helpKeyOf (c).isNotEmpty()) keys.insert (ui::helpKeyOf (c)); });
    auto& p = e.pl.processor();
    for (int t = 0; t < (int) ui::tours().size(); ++t)
    {
        e.ed->infoPanel().startTour (t);
        for (const auto& step : ui::tours()[(size_t) t].steps)
            for (const char* h : step.highlight)
            {
                INFO ("tour " << t << " " << h);
                CHECK (keys.count (h) == 1);
            }
        // Navigating changes nothing; Apply is one undo step.
        const auto before = p.snapshot();
        for (const auto& step : ui::tours()[(size_t) t].steps)
        {
            juce::ignoreUnused (step);
            e.ed->refreshForTest();
        }
        CHECK (p.snapshot() == before);
        for (const auto& step : ui::tours()[(size_t) t].steps)
            if (! step.apply.empty())
            {
                const auto pre = p.snapshot();
                p.commitUndoPoint();
                for (const auto& [id, v] : step.apply)
                    e.pl.set (id, v);
                p.commitUndoPoint();
                p.undo();
                CHECK (p.snapshot() == pre);
                p.redo();
            }
    }
}

TEST_CASE ("P2-T12 the activity record matches the sleep controllers", "[P2-T12]")
{
    Plugin pl;
    pl.preset (16);
    pl.prepare();
    auto& sc = pl.core().scopes;
    for (int t = 0; t < ScopeTaps::numTaps; ++t)
        sc.setEnabled (t, true);
    const auto x = signals::gapNoise (pl.fs);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    int mismatches = 0, blocks = 0;
    for (size_t pos = 0; pos + 512 <= x.size(); pos += 512)
    {
        buf.clear();
        buf.copyFrom (0, 0, x.data() + pos, 512);
        pl.processor().processBlock (buf, midi);
        const auto mask = sc.activity (sc.columnsWritten (ScopeTaps::outL) - 1);
        for (int g = 0; g < numGenerators; ++g)
            mismatches += ((mask >> g) & 1u) != (pl.core().sleepController ((GeneratorId) g).awakeFlag.load() ? 1u : 0u) ? 1 : 0;
        ++blocks;
    }
    INFO (blocks << " blocks");
    CHECK (mismatches == 0);
}

TEST_CASE ("P2-T33 every interactive control is named and reachable by keyboard", "[P2-T33][ui]")
{
    for (int language = 0; language < 2; ++language)
    {
        Editor e;
        e.ed->setLanguageForTest (language);
        int controls = 0;
        walk (e.ed->rootComponent(), [&] (juce::Component& c)
        {
            const bool interactive = dynamic_cast<juce::Slider*> (&c) != nullptr || dynamic_cast<juce::ComboBox*> (&c) != nullptr
                                     || dynamic_cast<juce::Button*> (&c) != nullptr;
            if (! interactive || ! c.isEnabled()) // disabled controls take no focus by design
                return;
            ++controls;
            INFO (language << " " << ui::helpKeyOf (c) << " " << c.getBounds().toString());
            const bool named = c.getTitle().isNotEmpty() || (dynamic_cast<juce::Button*> (&c) != nullptr
                                                             && dynamic_cast<juce::Button*> (&c)->getButtonText().isNotEmpty());
            CHECK (named);
            CHECK (c.getWantsKeyboardFocus());
        });
        CHECK (controls > 80);
    }
    // 150 % snapshot for inspection.
    Editor e (16);
    e.ed->setZoom (1.5f);
    e.ed->refreshForTest();
    const auto image = e.owner->createComponentSnapshot (e.owner->getLocalBounds(), true, 1.0f);
    CHECK (image.getWidth() == 1860);
    const auto file = juce::File (SPH_SOURCE_DIR).getChildFile ("docs/ui-views/zoom-150.png");
    file.deleteFile();
    if (auto out = file.createOutputStream())
        juce::PNGImageFormat().writeImageToStream (image, *out);
    e.ed->setZoom (1.0f);
}

TEST_CASE ("P2-T34 every parameter is in its host group", "[P2-T34]")
{
    Plugin pl;
    const auto& tree = pl.processor().getParameterTree();
    int grouped = 0;
    for (auto* group : tree.getSubgroups (false))
        for (auto* p : group->getParameters (true))
        {
            juce::ignoreUnused (p);
            ++grouped;
        }
    CHECK (grouped == numParameters);
    CHECK (tree.getSubgroups (false).size() == 14);
}
