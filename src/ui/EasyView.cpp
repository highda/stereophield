#include "ui/EasyView.h"

#include "plugin/Parameters.h"
#include "plugin/PluginProcessor.h"

namespace sph::ui
{
namespace
{
// What the macros move, in signal-flow order.
const char* hoodIds[] = { ids::width, ids::spread_amount, ids::velvet_amount, ids::coh_amount, ids::dbl_amount, ids::mod_amount,
                          ids::pan_amount, ids::room_amount, ids::room_size, ids::coh_spacing_cm, ids::bass_mono_hz,
                          ids::transient_duck, ids::band_low, ids::guard_ceiling_db, ids::engine };
} // namespace

EasyView::EasyView (StereophieldProcessor& p, OutputAnalysis& analysis)
    : proc (p),
      width (p.state(), ids::easy_width, true),
      character (p.state(), ids::easy_character),
      space (p.state(), ids::easy_space),
      focus (p.state(), ids::easy_focus),
      adapt (p.state(), ids::easy_adapt, "cap.easy_adapt"),
      lowLatency (p.state(), ids::easy_low_latency, "cap.easy_low_latency")
{
    for (auto* pnl : { &macroPanel, &meterPanel, &hoodPanel })
        addAndMakeVisible (*pnl);
    for (auto* k : { &width, &character, &space, &focus })
        macroPanel.addAndMakeVisible (*k);
    macroPanel.addAndMakeVisible (adapt);
    macroPanel.addAndMakeVisible (lowLatency);
    tagline.setFont (bodyFont());
    tagline.setJustificationType (juce::Justification::centred);
    tagline.setColour (juce::Label::textColourId, colours::secondary);
    setHelpKey (tagline, "ui.easy");
    macroPanel.addAndMakeVisible (tagline);

    meterPanel.addAndMakeVisible (goniometer);
    meterPanel.addAndMakeVisible (correlation);
    correlationLabel.setFont (labelFont());
    correlationLabel.setColour (juce::Label::textColourId, colours::secondary);
    meterPanel.addAndMakeVisible (correlationLabel);
    setHelpKey (goniometer, "disp.goniometer");
    setHelpKey (correlation, "disp.correlation");
    setHelpKey (correlationLabel, "disp.correlation");
    asw = std::make_unique<AswMeter> (analysis);
    meterPanel.addAndMakeVisible (*asw);
    setHelpKey (material, "disp.material");
    meterPanel.addAndMakeVisible (material);

    for (const char* id : hoodIds)
    {
        auto row = std::make_unique<HoodRow>();
        row->id = id;
        setHelpKey (*row, "p." + juce::String (id));
        hoodPanel.addAndMakeVisible (*row);
        rows.push_back (std::move (row));
    }
    localise();
}

EasyView::~EasyView() = default;

void EasyView::localise()
{
    tagline.setText (tr ("ui.easytagline"), juce::dontSendNotification);
    correlationLabel.setText (tr ("ui.correlation"), juce::dontSendNotification);
    repaint();
}

void EasyView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void EasyView::resized()
{
    const int gap = 6;
    auto r = getLocalBounds();
    auto top = r.removeFromTop (480);
    r.removeFromTop (gap);
    hoodPanel.setBounds (r);
    meterPanel.setBounds (top.removeFromRight (330));
    top.removeFromRight (gap);
    macroPanel.setBounds (top);

    {
        auto m = macroPanel.content();
        tagline.setBounds (m.removeFromTop (22));
        m.removeFromTop (6);
        auto big = m.removeFromTop (250);
        width.setBounds (big.withSizeKeepingCentre (250, 250));
        m.removeFromTop (18);
        auto row = m.removeFromTop (100);
        const int kw = 110;
        auto knobs = row.withSizeKeepingCentre (3 * kw + 2 * 30, row.getHeight());
        character.setBounds (knobs.removeFromLeft (kw));
        knobs.removeFromLeft (30);
        space.setBounds (knobs.removeFromLeft (kw));
        knobs.removeFromLeft (30);
        focus.setBounds (knobs.removeFromLeft (kw));
        m.removeFromTop (18);
        auto buttons = m.removeFromTop (26).withSizeKeepingCentre (2 * 170 + 12, 26);
        adapt.setBounds (buttons.removeFromLeft (170));
        buttons.removeFromLeft (12);
        lowLatency.setBounds (buttons);
    }
    {
        auto m = meterPanel.content();
        goniometer.setBounds (m.removeFromTop (260).withSizeKeepingCentre (260, 260));
        m.removeFromTop (4);
        auto c = m.removeFromTop (18);
        correlationLabel.setBounds (c.removeFromLeft (64));
        correlation.setBounds (c);
        m.removeFromTop (4);
        material.setBounds (m.removeFromBottom (54));
        m.removeFromBottom (4);
        asw->setBounds (m);
    }
    {
        auto h = hoodPanel.content();
        const int cols = 3, perCol = ((int) rows.size() + cols - 1) / cols;
        const int cw = (h.getWidth() - (cols - 1) * 12) / cols;
        const int rh = std::min (40, h.getHeight() / perCol);
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const int col = (int) i / perCol, line = (int) i % perCol;
            rows[i]->setBounds (h.getX() + col * (cw + 12), h.getY() + line * rh, cw, rh - 6);
        }
    }
}

void EasyView::refresh (const float* pairs, int n, double now)
{
    goniometer.push (pairs, n, now);
    goniometer.prune (now);
    goniometer.repaint();
    correlation.setValue (proc.core().meters.correlation());
    asw->repaint();

    const auto v = proc.effectiveValues();
    const auto macros = easy::readMacros (v.data());
    material.w = proc.materialWeights();
    material.adaptive = macros.adapt;
    material.repaint();
    for (auto& row : rows)
    {
        auto* p = proc.state().getParameter (row->id);
        const float plain = v[(size_t) indexOf (row->id.toRawUTF8())];
        const float norm = p->convertTo0to1 (plain);
        juce::String text;
        if (dynamic_cast<juce::AudioParameterChoice*> (p) != nullptr)
            text = tr ("choice." + row->id + "." + juce::String ((int) plain));
        else
            text = localNumber (p->getText (norm, 0));
        const bool active = ! (row->id == ids::coh_amount || row->id == ids::pan_amount || row->id == ids::coh_spacing_cm) || ! macros.lowLatency;
        if (std::abs (norm - row->norm) > 1e-4f || text != row->text || active != row->active)
        {
            row->norm = norm;
            row->text = text;
            row->active = active;
            row->repaint();
        }
    }
}

void EasyView::Material::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setFont (labelFont());
    g.setColour (colours::secondary);
    g.drawText (tr (adaptive ? "ui.material" : "ui.materialoff"), r.removeFromTop (14), juce::Justification::centredLeft);
    const char* keys[] = { "ui.percussive", "ui.tonal", "ui.mixed" };
    const int bw = (r.getWidth() - 8) / 3;
    for (int k = 0; k < 3; ++k)
    {
        auto b = r.removeFromLeft (bw);
        r.removeFromLeft (4);
        g.setColour (colours::secondary);
        g.drawText (tr (keys[k]), b.removeFromTop (16), juce::Justification::centredLeft);
        auto bar = b.removeFromTop (10).toFloat();
        g.setColour (colours::track);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (colours::accent.withAlpha (adaptive ? 1.0f : 0.4f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, w[(size_t) k])), 3.0f);
    }
}

void EasyView::HoodRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    const float alpha = active ? 1.0f : 0.4f;
    g.setFont (labelFont());
    auto line = r.removeFromTop (16);
    g.setColour (colours::text.withAlpha (alpha));
    g.drawText (tr ("hood." + id), line, juce::Justification::centredLeft);
    g.setColour (colours::secondary.withAlpha (alpha));
    g.drawText (text, line, juce::Justification::centredRight);
    auto bar = r.removeFromTop (8).toFloat().withTrimmedTop (2);
    g.setColour (colours::track);
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (colours::accent.withAlpha (0.8f * alpha));
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, norm)), 2.0f);
}
} // namespace sph::ui
