#include "ui/PluginEditor.h"

#include "plugin/Parameters.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Presets.h"

namespace sph
{
using namespace ui;

namespace
{
// Lays out knobs in a grid of the given columns inside r, skipping hidden ones.
void grid (juce::Rectangle<int> r, int columns, int cellHeight, std::initializer_list<juce::Component*> items)
{
    const int cw = r.getWidth() / columns;
    int i = 0;
    for (auto* c : items)
    {
        if (! c->isVisible())
            continue;
        c->setBounds (r.getX() + (i % columns) * cw, r.getY() + (i / columns) * cellHeight, cw, cellHeight);
        ++i;
    }
}

void row (juce::Rectangle<int> r, int cellWidth, std::initializer_list<juce::Component*> items)
{
    for (auto* c : items)
        c->setBounds (r.removeFromLeft (cellWidth));
}

void dim (juce::Component& c, bool enabled)
{
    c.setEnabled (enabled);
    c.setAlpha (enabled ? 1.0f : 0.4f);
}
} // namespace

PluginEditor::PluginEditor (StereophieldProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.state();
    auto knob = [&] (std::unique_ptr<Knob>& k, Panel& parent, const char* id, const char* name, bool big = false)
    {
        k = std::make_unique<Knob> (s, id, name, big);
        parent.addAndMakeVisible (*k);
    };
    auto choice = [&] (std::unique_ptr<Choice>& c, Panel& parent, const char* id, const char* caption)
    {
        c = std::make_unique<Choice> (s, id, caption);
        parent.addAndMakeVisible (*c);
    };

    // Header.
    const auto& presets = factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        presetMenu.addItem (presets[(size_t) i].name, i + 1);
    presetMenu.setTextWhenNothingSelected ("Presets");
    presetMenu.onChange = [this]
    {
        const int i = presetMenu.getSelectedId() - 1;
        if (i >= 0 && i != proc.getCurrentProgram())
            proc.setCurrentProgram (i);
        lastProgram = i;
    };
    presetMenu.setTitle ("Preset");
    addAndMakeVisible (presetMenu);
    engineLabel.setText ("Engine", juce::dontSendNotification);
    engineLabel.setFont (labelFont());
    engineLabel.setColour (juce::Label::textColourId, colours::secondary);
    addAndMakeVisible (engineLabel);
    engine = std::make_unique<Segmented> (s, ids::engine, juce::StringArray { "Light", "Full" });
    addAndMakeVisible (*engine);
    latencyLabel.setFont (bodyFont());
    latencyLabel.setColour (juce::Label::textColourId, colours::secondary);
    addAndMakeVisible (latencyLabel);
    bypass = std::make_unique<Toggle> (s, ids::bypass, "Bypass");
    addAndMakeVisible (*bypass);

    // Generators.
    for (auto* c : { &spreadCard, &delayCard, &modCard, &velvetCard, &panCard })
    {
        c->showIndicator (true);
        addAndMakeVisible (*c);
    }
    knob (spreadAmount, spreadCard, ids::spread_amount, "amount", true);
    choice (spreadSource, spreadCard, ids::spread_source, "source");
    choice (spreadType, spreadCard, ids::spread_type, "type");
    knob (spreadTime, spreadCard, ids::spread_time_ms, "time");
    knob (spreadDensity, spreadCard, ids::spread_density, "density");
    knob (spreadLo, spreadCard, ids::spread_f_lo, "low");
    knob (spreadHi, spreadCard, ids::spread_f_hi, "high");
    knob (spreadSkew, spreadCard, ids::spread_skew, "skew");
    knob (spreadQ, spreadCard, ids::spread_q, "Q");

    knob (delayAmount, delayCard, ids::delay_amount, "amount", true);
    choice (delaySource, delayCard, ids::delay_source, "source");
    choice (delaySide, delayCard, ids::delay_side, "side");
    knob (delayTime, delayCard, ids::delay_time_ms, "time");
    knob (delayLp, delayCard, ids::delay_lp_hz, "low-pass");

    knob (modAmount, modCard, ids::mod_amount, "amount", true);
    choice (modSource, modCard, ids::mod_source, "source");
    choice (modType, modCard, ids::mod_type, "type");
    knob (modRate, modCard, ids::mod_rate_hz, "rate");
    knob (modDepth, modCard, ids::mod_depth_ms, "depth");
    knob (modBase, modCard, ids::mod_base_ms, "base");
    knob (modCents, modCard, ids::mod_cents, "cents");
    knob (modPredelay, modCard, ids::mod_predelay_ms, "pre-delay");

    knob (velvetAmount, velvetCard, ids::velvet_amount, "amount", true);
    choice (velvetSource, velvetCard, ids::velvet_source, "source");
    knob (velvetSize, velvetCard, ids::velvet_size_ms, "size");
    knob (velvetDensity, velvetCard, ids::velvet_density, "density");
    knob (velvetVariation, velvetCard, ids::velvet_variation, "variation");

    knob (panAmount, panCard, ids::pan_amount, "amount", true);
    choice (panMode, panCard, ids::pan_mode, "mode");
    knob (panDepth, panCard, ids::pan_depth, "depth");
    knob (panDensity, panCard, ids::pan_density, "density");
    knob (panBass, panCard, ids::pan_bass_center_hz, "bass ctr");
    knob (panGroups, panCard, ids::pan_max_groups, "groups");

    // Meters.
    addAndMakeVisible (meterPanel);
    meterPanel.addAndMakeVisible (goniometer);
    meterPanel.addAndMakeVisible (correlation);
    meterPanel.addAndMakeVisible (levels);
    correlationLabel.setText ("correlation", juce::dontSendNotification);
    correlationLabel.setFont (labelFont());
    correlationLabel.setColour (juce::Label::textColourId, colours::secondary);
    meterPanel.addAndMakeVisible (correlationLabel);

    addAndMakeVisible (panDisplayPanel);
    panDisplayPanel.addAndMakeVisible (panDisplay);

    // Bottom row.
    for (auto* c : { &analysisPanel, &sidePanel, &outputPanel })
        addAndMakeVisible (*c);
    knob (ambience, analysisPanel, ids::ambience, "ambience");
    knob (roomDecay, analysisPanel, ids::room_decay_s, "room decay");
    knob (bassMono, sidePanel, ids::bass_mono_hz, "bass mono");
    knob (xoverLo, sidePanel, ids::band_xover_lo, "xover lo");
    knob (xoverHi, sidePanel, ids::band_xover_hi, "xover hi");
    knob (bandLow, sidePanel, ids::band_low, "low");
    knob (bandMid, sidePanel, ids::band_mid, "mid");
    knob (bandHigh, sidePanel, ids::band_high, "high");
    knob (duck, sidePanel, ids::transient_duck, "duck");
    guard = std::make_unique<Toggle> (s, ids::guard, "guard");
    sidePanel.addAndMakeVisible (*guard);
    guardLabel.setText ("correlation guard", juce::dontSendNotification);
    guardLabel.setFont (labelFont());
    guardLabel.setJustificationType (juce::Justification::centred);
    guardLabel.setColour (juce::Label::textColourId, colours::secondary);
    sidePanel.addAndMakeVisible (guardLabel);

    knob (widthKnob, outputPanel, ids::width, "WIDTH", true);
    knob (midBlend, outputPanel, ids::mid_blend, "mid blend");
    knob (outGain, outputPanel, ids::out_gain_db, "gain");
    choice (compMode, outputPanel, ids::comp_mode, "comp");
    listen = std::make_unique<Segmented> (s, ids::listen, juce::StringArray { "Stereo", "Mono", "Side" });
    outputPanel.addAndMakeVisible (*listen);
    monoWarning.setText ("not mono-safe", juce::dontSendNotification);
    monoWarning.setFont (labelFont());
    monoWarning.setColour (juce::Label::textColourId, colours::warning);
    monoWarning.setJustificationType (juce::Justification::centredLeft);
    outputPanel.addChildComponent (monoWarning);

    gonioBuffer.assign ((size_t) (2 * Meters::fifoPairs), 0.0f);
    // Children created before joining the hierarchy rebuild their parts
    // (slider text boxes) with this look and feel.
    sendLookAndFeelChange();
    setResizable (false, false);
    setSize (width, height);
    updateVisibility();
    timerCallback();
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

float PluginEditor::param (const char* id) const
{
    return proc.state().getRawParameterValue (id)->load();
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    auto head = getLocalBounds().removeFromTop (48).reduced (16, 0);
    g.setColour (colours::text);
    g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    g.drawText ("stereophield", head.removeFromLeft (130), juce::Justification::centredLeft);
}

void PluginEditor::updateVisibility()
{
    const bool cascade = (int) param (ids::spread_type) == 1;
    spreadTime->setVisible (! cascade);
    for (auto* k : { spreadDensity.get(), spreadLo.get(), spreadHi.get(), spreadSkew.get(), spreadQ.get() })
        k->setVisible (cascade);

    const bool chorus = (int) param (ids::mod_type) == 0;
    for (auto* k : { modRate.get(), modDepth.get(), modBase.get() })
        k->setVisible (chorus);
    for (auto* k : { modCents.get(), modPredelay.get() })
        k->setVisible (! chorus);

    const int mode = (int) param (ids::pan_mode);
    panDensity->setVisible (mode != 2);
    panGroups->setVisible (mode == 2);

    const bool full = (int) param (ids::engine) == 1;
    dim (panCard, full);
    dim (analysisPanel, full);
    for (auto* c : { spreadSource.get(), delaySource.get(), modSource.get(), velvetSource.get() })
        dim (*c, full);
}

void PluginEditor::resized()
{
    // Header, 48 pt.
    auto head = getLocalBounds().removeFromTop (48).reduced (16, 10);
    head.removeFromLeft (130);
    presetMenu.setBounds (head.removeFromLeft (230));
    head.removeFromLeft (24);
    engineLabel.setBounds (head.removeFromLeft (48));
    engine->setBounds (head.removeFromLeft (120));
    head.removeFromLeft (20);
    bypass->setBounds (head.removeFromRight (80));
    latencyLabel.setBounds (head.removeFromLeft (180));

    // Main row: five cards and the meter column.
    const int gap = 6, top = 54, mainH = 316;
    auto main = juce::Rectangle<int> (gap, top, width - 2 * gap, mainH);
    meterPanel.setBounds (main.removeFromRight (268));
    main.removeFromRight (gap);
    const int cardW = (main.getWidth() - 4 * gap) / 5;
    ui::Panel* cards[] = { &spreadCard, &delayCard, &modCard, &velvetCard, &panCard };
    for (int i = 0; i < 5; ++i)
    {
        cards[i]->setBounds (main.removeFromLeft (i == 4 ? main.getWidth() : cardW));
        main.removeFromLeft (gap);
    }

    auto card = [] (Panel& p, Knob& amount, std::initializer_list<Choice*> choices, std::initializer_list<juce::Component*> knobs)
    {
        auto r = p.content();
        amount.setBounds (r.removeFromTop (62).withSizeKeepingCentre (76, 62));
        r.removeFromTop (4);
        for (auto* c : choices)
        {
            c->setBounds (r.removeFromTop (22));
            r.removeFromTop (4);
        }
        r.removeFromTop (2);
        grid (r, 2, 52, knobs);
    };
    card (spreadCard, *spreadAmount, { spreadSource.get(), spreadType.get() },
          { spreadTime.get(), spreadDensity.get(), spreadLo.get(), spreadHi.get(), spreadSkew.get(), spreadQ.get() });
    card (delayCard, *delayAmount, { delaySource.get(), delaySide.get() }, { delayTime.get(), delayLp.get() });
    card (modCard, *modAmount, { modSource.get(), modType.get() },
          { modRate.get(), modDepth.get(), modBase.get(), modCents.get(), modPredelay.get() });
    card (velvetCard, *velvetAmount, { velvetSource.get() }, { velvetSize.get(), velvetDensity.get(), velvetVariation.get() });
    card (panCard, *panAmount, { panMode.get() }, { panDepth.get(), panDensity.get(), panBass.get(), panGroups.get() });

    // Meter column: goniometer 260 x 260, correlation, levels.
    {
        auto r = meterPanel.getLocalBounds().reduced (4);
        goniometer.setBounds (r.removeFromTop (260).withSizeKeepingCentre (260, 260));
        r.removeFromTop (2);
        auto c = r.removeFromTop (20);
        correlationLabel.setBounds (c.removeFromLeft (66));
        correlation.setBounds (c);
        r.removeFromTop (2);
        levels.setBounds (r);
    }

    // Pan map display, 120 pt.
    panDisplayPanel.setBounds (gap, top + mainH + gap, width - 2 * gap, 120);
    panDisplay.setBounds (panDisplayPanel.content().withTrimmedTop (-4));

    // Bottom row.
    auto bottom = juce::Rectangle<int> (gap, top + mainH + gap + 120 + gap, width - 2 * gap, 0);
    bottom.setBottom (height - gap);
    analysisPanel.setBounds (bottom.removeFromLeft (150));
    bottom.removeFromLeft (gap);
    outputPanel.setBounds (bottom.removeFromRight (352));
    bottom.removeFromRight (gap);
    sidePanel.setBounds (bottom);

    row (analysisPanel.content(), 67, { ambience.get(), roomDecay.get() });
    {
        auto r = sidePanel.content();
        auto g = r.removeFromRight (70);
        row (r, r.getWidth() / 7, { bassMono.get(), xoverLo.get(), xoverHi.get(), bandLow.get(), bandMid.get(), bandHigh.get(), duck.get() });
        guardLabel.setBounds (g.removeFromTop (30).withTrimmedTop (8));
        guard->setBounds (g.removeFromTop (26).reduced (6, 0));
    }
    {
        auto r = outputPanel.content();
        widthKnob->setBounds (r.removeFromLeft (80));
        midBlend->setBounds (r.removeFromLeft (64));
        outGain->setBounds (r.removeFromLeft (64));
        r.removeFromLeft (4);
        monoWarning.setBounds (r.removeFromTop (20));
        r.removeFromTop (4);
        compMode->setBounds (r.removeFromTop (22));
        r.removeFromTop (8);
        listen->setBounds (r.removeFromTop (24));
    }
}

void PluginEditor::timerCallback()
{
    // Layout follows the selected types and engine.
    const int key = (int) param (ids::spread_type) | ((int) param (ids::mod_type) << 1) | ((int) param (ids::pan_mode) << 2)
                    | ((int) param (ids::engine) << 4);
    if (key != layoutKey)
    {
        layoutKey = key;
        updateVisibility();
        resized();
    }

    const int program = proc.getCurrentProgram();
    if (program != lastProgram)
    {
        lastProgram = program;
        presetMenu.setSelectedId (program + 1, juce::dontSendNotification);
    }

    const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    latencyLabel.setText ("Latency: " + juce::String (1000.0 * proc.getLatencySamples() / fs, 1) + " ms",
                          juce::dontSendNotification);
    monoWarning.setVisible (param (ids::mid_blend) > 0.0f);

    auto& core = proc.core();
    Panel* cards[] = { &spreadCard, &delayCard, &modCard, &velvetCard, &panCard };
    for (int g = 0; g < (int) std::size (cards); ++g)
        cards[g]->setAwake (core.sleepController ((GeneratorId) g).awakeFlag.load (std::memory_order_relaxed));

    const double now = juce::Time::getMillisecondCounterHiRes();
    auto& meters = core.meters;
    const int n = meters.popGoniometer (gonioBuffer.data(), Meters::fifoPairs);
    goniometer.push (gonioBuffer.data(), n, now);
    goniometer.prune (now);
    goniometer.repaint();
    correlation.setValue (meters.correlation());
    levels.setPeaks (meters.takePeak (0), meters.takePeak (1), now);
    panDisplay.update (core.panMap(), (int) param (ids::engine) == 1);
}
} // namespace sph
