#include "ui/PluginEditor.h"

#include "plugin/Parameters.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Presets.h"

#include <cmath>

namespace sph
{
using namespace ui;

namespace
{
const char* cardKeys[] = { "card.spread", "card.delay", "card.mod", "card.velvet", "card.pan", "card.coherence", "card.double", "card.room", "card.image" };
const char* cardAmounts[] = { ids::spread_amount, ids::delay_amount, ids::mod_amount, ids::velvet_amount, ids::pan_amount,
                              ids::coh_amount, ids::dbl_amount, ids::room_amount, ids::img_amount };
const char* cardSources[] = { ids::spread_source, ids::delay_source, ids::mod_source, ids::velvet_source, ids::pan_mode,
                              ids::coh_source, ids::dbl_source, ids::room_source, nullptr };
const int cardTaps[] = { ScopeTaps::genSpread, ScopeTaps::genDelay, ScopeTaps::genMod, ScopeTaps::genVelvet, ScopeTaps::genPan,
                         ScopeTaps::genCoherence, ScopeTaps::genDouble, ScopeTaps::genRoom, ScopeTaps::inS };
const std::vector<std::vector<const char*>> detailIds {
    { ids::spread_type, ids::spread_time_ms, ids::spread_density, ids::spread_f_lo, ids::spread_f_hi, ids::spread_skew, ids::spread_q },
    { ids::delay_side, ids::delay_time_ms, ids::delay_lp_hz },
    { ids::mod_type, ids::mod_rate_hz, ids::mod_depth_ms, ids::mod_base_ms, ids::mod_cents, ids::mod_predelay_ms },
    { ids::velvet_design, ids::velvet_size_ms, ids::velvet_density, ids::velvet_variation },
    { ids::pan_ownership, ids::pan_depth, ids::pan_density, ids::pan_bass_center_hz, ids::pan_max_groups },
    { ids::coh_mode, ids::coh_pattern, ids::coh_transient, ids::coh_spacing_cm, ids::coh_angle_deg,
      ids::coh_p63, ids::coh_p250, ids::coh_p1k, ids::coh_p4k, ids::coh_p16k },
    { ids::dbl_offset_ms, ids::dbl_drift_ms, ids::dbl_drift_rate, ids::dbl_pitch_cents, ids::dbl_level_db, ids::dbl_tone_db, ids::dbl_seed },
    { ids::room_order, ids::room_size, ids::room_distance, ids::room_absorb, ids::room_damp_hz },
    { ids::img_diffuse, ids::img_center_hz },
};
const char* tabKeys[] = { "tab.details", "tab.pan", "tab.coherence", "tab.flow", "tab.scopes", "tab.bands" };

bool isChoice (APVTS& s, const char* id) { return dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)) != nullptr; }

void dim (juce::Component& c, bool enabled)
{
    c.setEnabled (enabled);
    c.setAlpha (enabled ? 1.0f : 0.4f);
}

juce::String fmtN (double v, int d) { return localNumber (juce::String (v, d)); }
juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }
bool cz() { return language() == Language::Czech; }
} // namespace

PluginEditor::Confirm::Confirm()
{
    title.setFont (titleFont());
    title.setColour (juce::Label::textColourId, colours::text);
    body.setFont (bodyFont());
    body.setColour (juce::Label::textColourId, colours::secondary);
    body.setJustificationType (juce::Justification::topLeft);
    for (auto* c : std::initializer_list<juce::Component*> { &title, &body, &ok, &cancel })
        addAndMakeVisible (*c);
}

void PluginEditor::Confirm::paint (juce::Graphics& g)
{
    g.fillAll (colours::background.withAlpha (0.7f));
    auto box = getLocalBounds().withSizeKeepingCentre (420, 132).toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (box, cornerRadius);
    g.setColour (colours::accent.withAlpha (0.8f));
    g.drawRoundedRectangle (box.reduced (0.75f), cornerRadius, 1.5f);
}

void PluginEditor::Confirm::resized()
{
    auto box = getLocalBounds().withSizeKeepingCentre (420, 132).reduced (16, 14);
    title.setBounds (box.removeFromTop (22));
    auto buttons = box.removeFromBottom (28);
    cancel.setBounds (buttons.removeFromRight (110));
    buttons.removeFromRight (8);
    ok.setBounds (buttons.removeFromRight (150));
    box.removeFromBottom (8);
    body.setBounds (box);
}

PluginEditor::PluginEditor (StereophieldProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    setLanguage (proc.uiLanguage() == 1 ? Language::Czech : Language::English);
    appliedLanguage = proc.uiLanguage();
    auto& s = proc.state();
    addAndMakeVisible (root);

    // ---------------------------------------------------------- header
    root.addAndMakeVisible (presetMenu);
    setHelpKey (presetMenu, "ui.presets");
    presetMenu.onChange = [this]
    {
        const int id = presetMenu.getSelectedId();
        if (id >= 1 && id <= (int) factoryPresets().size())
        {
            if (id - 1 != proc.getCurrentProgram())
                proc.setCurrentProgram (id - 1);
        }
        else if (id >= 1000)
            proc.loadUserPreset (proc.userPresets()[id - 1000]);
        lastProgram = proc.getCurrentProgram();
    };
    prevPreset.onClick = [this] { proc.setCurrentProgram (juce::jmax (0, proc.getCurrentProgram() - 1)); };
    nextPreset.onClick = [this] { proc.setCurrentProgram (juce::jmin ((int) factoryPresets().size() - 1, proc.getCurrentProgram() + 1)); };
    undoButton.onClick = [this] { proc.undo(); };
    redoButton.onClick = [this] { proc.redo(); };
    slotA.setClickingTogglesState (false);
    slotA.onClick = [this] { proc.selectCompareSlot (0); };
    slotB.onClick = [this] { proc.selectCompareSlot (1); };
    copyAB.onClick = [this] { proc.copyCompareAToB(); };
    infoButton.onClick = [this] { setInfoVisible (! infoOpen); };
    learnButton.onClick = [this] { if (! infoOpen) setInfoVisible (true); info.openTourList(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };
    for (auto* b : { &prevPreset, &nextPreset, &undoButton, &redoButton, &slotA, &slotB, &copyAB, &infoButton, &learnButton, &settingsButton })
        root.addAndMakeVisible (*b);
    setHelpKey (prevPreset, "ui.presets");
    setHelpKey (nextPreset, "ui.presets");
    setHelpKey (undoButton, "ui.undo");
    setHelpKey (redoButton, "ui.redo");
    for (auto* b : { &slotA, &slotB, &copyAB })
        setHelpKey (*b, "ui.ab");
    setHelpKey (learnButton, "ui.learn");
    setHelpKey (infoButton, "ui.learn");
    engineLabel.setFont (labelFont());
    engineLabel.setColour (juce::Label::textColourId, colours::secondary);
    root.addAndMakeVisible (engineLabel);
    engine = std::make_unique<Segmented> (&s, ids::engine, std::vector<juce::String> { "choice.engine.0", "choice.engine.1" });
    root.addAndMakeVisible (*engine);
    latencyLabel.setFont (bodyFont());
    latencyLabel.setColour (juce::Label::textColourId, colours::secondary);
    setHelpKey (latencyLabel, "p.latency_mode");
    root.addAndMakeVisible (latencyLabel);
    languageSwitch = std::make_unique<Segmented> (nullptr, juce::String(), std::vector<juce::String> { "lang.en", "lang.cs" });
    languageSwitch->select (proc.uiLanguage());
    languageSwitch->onSelect = [this] (int l)
    {
        proc.setUiLanguage (l);
        applyLanguage();
    };
    setHelpKey (*languageSwitch, "ui.language");
    root.addAndMakeVisible (*languageSwitch);
    bypass = std::make_unique<Toggle> (s, ids::bypass, "ui.bypass");
    root.addAndMakeVisible (*bypass);
    modeButton.onClick = [this]
    {
        if (proc.isEasyMode())
            proc.expandToComplete();
        else
            askCollapse();
        applyMode();
    };
    root.addAndMakeVisible (modeButton);

    // ----------------------------------------------------------- cards
    for (int c = 0; c < numCards; ++c)
    {
        cards[(size_t) c] = std::make_unique<Panel> (cardKeys[c]);
        auto& card = *cards[(size_t) c];
        card.showIndicator (c < 8);
        card.onClick = [this, c] { selectGenerator (c); };
        root.addAndMakeVisible (card);
        cardAmount[(size_t) c] = std::make_unique<Knob> (s, cardAmounts[c], true);
        card.addAndMakeVisible (*cardAmount[(size_t) c]);
        if (cardSources[c] != nullptr)
        {
            cardSource[(size_t) c] = std::make_unique<Choice> (s, cardSources[c], false);
            card.addAndMakeVisible (*cardSource[(size_t) c]);
        }
        cardScope[(size_t) c] = std::make_unique<MiniScope> (proc.core().scopes, cardTaps[c]);
        cardScope[(size_t) c]->setInterceptsMouseClicks (false, false);
        card.addAndMakeVisible (*cardScope[(size_t) c]);

        for (const char* id : detailIds[(size_t) c])
        {
            std::unique_ptr<juce::Component> comp;
            if (isChoice (s, id))
                comp = std::make_unique<Choice> (s, id, true);
            else
                comp = std::make_unique<Knob> (s, id);
            byId[id] = comp.get();
            root.addChildComponent (*comp);
            details[(size_t) c].push_back (std::move (comp));
        }
    }

    // ---------------------------------------------------------- meters
    root.addAndMakeVisible (meterPanel);
    meterPanel.addAndMakeVisible (goniometer);
    meterPanel.addAndMakeVisible (correlation);
    meterPanel.addAndMakeVisible (levels);
    correlationLabel.setFont (labelFont());
    correlationLabel.setColour (juce::Label::textColourId, colours::secondary);
    meterPanel.addAndMakeVisible (correlationLabel);
    setHelpKey (goniometer, "disp.goniometer");
    setHelpKey (correlation, "disp.correlation");
    setHelpKey (correlationLabel, "disp.correlation");
    setHelpKey (levels, "disp.levels");
    analysis.prepare (proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);
    asw = std::make_unique<AswMeter> (analysis);
    meterPanel.addAndMakeVisible (*asw);

    // ---------------------------------------------------------- centre
    root.addAndMakeVisible (centre);
    for (int t = 0; t < numTabs; ++t)
    {
        auto& b = tabs[(size_t) t];
        b.setClickingTogglesState (false);
        b.setConnectedEdges ((t > 0 ? juce::Button::ConnectedOnLeft : 0) | (t + 1 < numTabs ? juce::Button::ConnectedOnRight : 0));
        b.onClick = [this, t] { selectTab (t); };
        setHelpKey (b, tabKeys[t]);
        root.addAndMakeVisible (b);
    }
    setHelpKey (panDisplay, "disp.pan");
    centre.addChildComponent (panDisplay);
    coherenceView = std::make_unique<CoherenceView> (s, proc.core().coherence(), proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);
    centre.addChildComponent (*coherenceView);
    flow = std::make_unique<FlowView> (proc.core().scopes, [this] (int g) { return proc.core().sleepController ((GeneratorId) g).awakeFlag.load(); });
    flow->onOpenTap = [this] (int t) { selectTab (tabScopes); scopes->showTap (t); };
    centre.addChildComponent (*flow);
    scopes = std::make_unique<ScopeLanes> (proc.core().scopes);
    centre.addChildComponent (*scopes);
    bands = std::make_unique<BandsView> (analysis);
    setHelpKey (*bands, "disp.bands");
    centre.addChildComponent (*bands);
    centreNote.setFont (labelFont());
    centreNote.setColour (juce::Label::textColourId, colours::secondary);
    centreNote.setJustificationType (juce::Justification::centred);
    centre.addChildComponent (centreNote);

    // ---------------------------------------------------------- bottom
    for (auto* pnl : { &analysisPanel, &sidePanel, &outputPanel })
        root.addAndMakeVisible (*pnl);
    auto bottom = [&] (Panel& parent, const char* id, bool big = false)
    {
        std::unique_ptr<juce::Component> c;
        if (isChoice (s, id))
            c = std::make_unique<Choice> (s, id, true);
        else
            c = std::make_unique<Knob> (s, id, big);
        byId[id] = c.get();
        parent.addAndMakeVisible (*c);
        bottomControls.push_back (std::move (c));
    };
    for (const char* id : { ids::ambience, ids::room_decay_s, ids::transient_mode, ids::latency_mode })
        bottom (analysisPanel, id);
    for (const char* id : { ids::bass_mono_hz, ids::band_xover_lo, ids::band_xover_hi, ids::band_low, ids::band_mid, ids::band_high, ids::transient_duck })
        bottom (sidePanel, id);
    {
        auto g = std::make_unique<Toggle> (s, ids::guard, "cap.guard");
        byId[ids::guard] = g.get();
        sidePanel.addAndMakeVisible (*g);
        bottomControls.push_back (std::move (g));
    }
    guardLabel.setFont (labelFont());
    guardLabel.setJustificationType (juce::Justification::centred);
    guardLabel.setColour (juce::Label::textColourId, colours::secondary);
    setHelpKey (guardLabel, "p.guard");
    sidePanel.addAndMakeVisible (guardLabel);
    guardCeiling.setSliderStyle (juce::Slider::LinearBar);
    guardCeiling.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 18);
    guardCeiling.setColour (juce::Slider::trackColourId, colours::accent.withAlpha (0.5f));
    guardCeilingAttachment = std::make_unique<APVTSAttachment> (s, ids::guard_ceiling_db, guardCeiling);
    guardCeiling.textFromValueFunction = [] (double v) { return localNumber (juce::String (v, 1)) + " dB"; };
    guardCeiling.updateText();
    setHelpKey (guardCeiling, "p.guard_ceiling_db");
    sidePanel.addAndMakeVisible (guardCeiling);
    bottom (outputPanel, ids::width, true);
    bottom (outputPanel, ids::mid_blend);
    bottom (outputPanel, ids::out_gain_db);
    bottom (outputPanel, ids::comp_mode);
    bottom (outputPanel, ids::width_mode);
    aswTarget.setSliderStyle (juce::Slider::LinearBar);
    aswTarget.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 20);
    aswTarget.setColour (juce::Slider::trackColourId, colours::accent.withAlpha (0.5f));
    aswAttachment = std::make_unique<APVTSAttachment> (s, ids::asw_target, aswTarget);
    setHelpKey (aswTarget, "p.asw_target");
    outputPanel.addAndMakeVisible (aswTarget);
    {
        auto l = std::make_unique<Segmented> (&s, ids::listen, std::vector<juce::String> { "choice.listen.0", "choice.listen.1", "choice.listen.2" });
        byId[ids::listen] = l.get();
        outputPanel.addAndMakeVisible (*l);
        bottomControls.push_back (std::move (l));
    }
    monoWarning.setFont (labelFont());
    monoWarning.setColour (juce::Label::textColourId, colours::warning);
    setHelpKey (monoWarning, "p.mid_blend");
    outputPanel.addChildComponent (monoWarning);

    // ------------------------------------------------------------ info
    root.addAndMakeVisible (info);
    info.onApply = [this] (const TourStep& st)
    {
        // Tours demonstrate the complete interface's controls.
        if (proc.isEasyMode())
        {
            proc.expandToComplete();
            applyMode();
        }
        proc.commitUndoPoint();
        for (const auto& [id, v] : st.apply)
            if (auto* prm = proc.state().getParameter (id))
                prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        proc.commitUndoPoint();
    };
    info.onTourChanged = [this] { overlay.repaint(); };

    // ------------------------------------------------------------ easy
    easy = std::make_unique<EasyView> (proc, analysis);
    root.addChildComponent (*easy);
    confirm.ok.onClick = [this]
    {
        confirm.setVisible (false);
        proc.collapseToEasy (true);
        applyMode();
    };
    confirm.cancel.onClick = [this] { confirm.setVisible (false); };
    root.addChildComponent (confirm);

    overlay.setInterceptsMouseClicks (false, false);
    root.addAndMakeVisible (overlay);

    // Every control can be reached and operated with the keyboard (I9).
    std::function<void (juce::Component&)> focusable = [&focusable] (juce::Component& c)
    {
        if (dynamic_cast<juce::Slider*> (&c) != nullptr || dynamic_cast<juce::ComboBox*> (&c) != nullptr
            || dynamic_cast<juce::Button*> (&c) != nullptr)
            c.setWantsKeyboardFocus (true);
        for (auto* ch : c.getChildren())
            focusable (*ch);
    };
    focusable (root);

    // The detail controls float above the centre panel; the tour overlay
    // above everything.
    for (auto& list : details)
        for (auto& comp : list)
            comp->toFront (false);
    easy->toFront (false);
    overlay.toFront (false);
    confirm.toFront (false);

    gonioBuffer.assign ((size_t) (2 * Meters::fifoPairs), 0.0f);
    for (int t = 0; t < ScopeTaps::numTaps; ++t)
        proc.core().scopes.setEnabled (t, true);
    proc.core().outputRing.setEnabled (true);
    proc.addChangeListener (this);

    zoom = (float) savedSetting ("zoom", 1.0);
    infoOpen = savedSetting ("info", 1.0) > 0.5;
    tooltipsWithInfo = savedSetting ("tooltipsWithInfo", 0.0) > 0.5;
    setResizable (false, false);
    sendLookAndFeelChange();
    applyMode();
    applyLanguage();
    selectGenerator (0);
    selectTab (tabDetails);
    setZoom (zoom);
    timerCallback();
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
    proc.core().outputRing.setEnabled (false);
    for (int t = 0; t < ScopeTaps::numTaps; ++t)
        proc.core().scopes.setEnabled (t, false);
    setLookAndFeel (nullptr);
}

float PluginEditor::param (const char* id) const
{
    if (auto* f = dynamic_cast<ExactFloatParameter*> (proc.state().getParameter (id)))
        return f->plain();
    return proc.state().getRawParameterValue (id)->load();
}

bool PluginEditor::stereoInput() const { return proc.getTotalNumInputChannels() >= 2; }

void PluginEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // The session set the language.
    if (proc.uiLanguage() != appliedLanguage)
        applyLanguage();
}

void PluginEditor::setLanguageForTest (int l)
{
    proc.setUiLanguage (l);
    applyLanguage();
}

void PluginEditor::applyLanguage()
{
    appliedLanguage = proc.uiLanguage();
    setLanguage (appliedLanguage == 1 ? Language::Czech : Language::English);
    saveLanguagePreference();
    languageSwitch->select (appliedLanguage);
    engineLabel.setText (tr ("ui.engine"), juce::dontSendNotification);
    undoButton.setButtonText (tr ("ui.undo"));
    redoButton.setButtonText (tr ("ui.redo"));
    copyAB.setButtonText (tr ("ui.copyab"));
    modeButton.setButtonText (tr (proc.isEasyMode() ? "ui.expand" : "ui.collapse"));
    modeButton.setTitle (modeButton.getButtonText());
    setHelpKey (modeButton, proc.isEasyMode() ? "ui.expand" : "ui.collapse");
    modeButton.setTooltip (shortHelp (helpKeyOf (modeButton)));
    confirm.title.setText (tr ("ui.collapsetitle"), juce::dontSendNotification);
    confirm.body.setText (tr ("ui.collapsebody"), juce::dontSendNotification);
    confirm.ok.setButtonText (tr ("ui.collapseok"));
    confirm.cancel.setButtonText (tr ("ui.cancel"));
    guardCeiling.setTitle (tr ("cap.guard_ceiling_db"));
    guardCeiling.updateText();
    infoButton.setButtonText (tr ("ui.info"));
    learnButton.setButtonText (tr ("ui.learn"));
    correlationLabel.setText (tr ("ui.correlation"), juce::dontSendNotification);
    guardLabel.setText (tr ("ui.guardlabel"), juce::dontSendNotification);
    monoWarning.setText (tr ("ui.notmonosafe"), juce::dontSendNotification);
    for (int t = 0; t < numTabs; ++t)
        tabs[(size_t) t].setButtonText (tr (tabKeys[t]));
    for (auto* b : { &prevPreset, &nextPreset, &undoButton, &redoButton, &slotA, &slotB, &copyAB, &infoButton, &learnButton, &settingsButton })
        b->setTooltip (shortHelp (helpKeyOf (*b)));
    for (int t = 0; t < numTabs; ++t)
        tabs[(size_t) t].setTitle (tr (tabKeys[t]));
    undoButton.setTitle (tr ("ui.undo"));
    redoButton.setTitle (tr ("ui.redo"));
    settingsButton.setTitle (tr ("ui.settings"));
    infoButton.setTitle (tr ("panel.info"));
    presetMenu.setTitle (tr ("ui.presets"));
    localiseTree (root);
    buildPresetMenu();
    layoutKey = -1;
    repaint();
}

void PluginEditor::buildPresetMenu()
{
    presetMenu.clear (juce::dontSendNotification);
    presetMenu.addSectionHeading (tr ("ui.factory"));
    const auto& presets = factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        Help h;
        const bool translated = help ("preset." + juce::String (i + 1), h);
        presetMenu.addItem (translated ? h.title : juce::String (presets[(size_t) i].name), i + 1);
    }
    const auto user = proc.userPresets();
    if (! user.isEmpty())
    {
        presetMenu.addSectionHeading (tr ("ui.user"));
        for (int i = 0; i < user.size(); ++i)
            presetMenu.addItem (user[i], 1000 + i);
    }
    presetMenu.setTextWhenNothingSelected (tr ("ui.presets"));
    presetMenu.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void PluginEditor::showSettingsMenu()
{
    juce::PopupMenu m, z, teach, del;
    for (float v : { 0.75f, 1.0f, 1.25f, 1.5f })
        z.addItem (juce::String ((int) std::lround (v * 100)) + " %", true, std::abs (zoom - v) < 0.01f, [this, v] { setZoom (v); });
    m.addSubMenu (tr ("ui.zoom"), z);
    m.addItem (tr ("ui.tooltips"), true, tooltipsWithInfo, [this]
    {
        tooltipsWithInfo = ! tooltipsWithInfo;
        saveSetting ("tooltipsWithInfo", tooltipsWithInfo ? 1.0 : 0.0);
        setInfoVisible (infoOpen);
    });
    for (int i = 0; i <= 5; ++i)
        teach.addItem (tr ("choice.teach." + juce::String (i)), true, proc.teachingSource() == i, [this, i] { proc.setTeachingSource (i); });
    m.addSubMenu (tr ("ui.teach"), teach);
    m.addSeparator();
    m.addItem (tr ("ui.save"), [this] { savePresetDialog(); });
    const auto user = proc.userPresets();
    for (const auto& u : user)
        del.addItem (u, [this, u] { proc.deleteUserPreset (u); buildPresetMenu(); });
    m.addSubMenu (tr ("ui.delete"), del, ! user.isEmpty());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (settingsButton));
}

void PluginEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow (tr ("ui.save"), tr ("ui.presetname"), juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&lnf);
    w->addTextEditor ("name", {});
    w->addButton (tr ("ui.ok"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (tr ("ui.cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result == 1)
            if (proc.saveUserPreset (w->getTextEditorContents ("name")))
                buildPresetMenu();
    }), true);
}

void PluginEditor::askCollapse()
{
    confirm.setBounds (root.getLocalBounds());
    confirm.setVisible (true);
    confirm.toFront (true);
    confirm.cancel.grabKeyboardFocus();
}

void PluginEditor::applyMode()
{
    easyMode = proc.isEasyMode();
    // The complete interface stays built underneath; Easy covers it.
    for (auto* c : std::initializer_list<juce::Component*> { &presetMenu, &prevPreset, &nextPreset, engine.get(), &meterPanel, &centre,
                                                             &analysisPanel, &sidePanel, &outputPanel })
        c->setVisible (! easyMode);
    engineLabel.setVisible (! easyMode && infoOpen);
    for (auto& card : cards)
        card->setVisible (! easyMode);
    for (auto& t : tabs)
        t.setVisible (! easyMode);
    easy->setVisible (easyMode);
    if (easyMode)
        for (auto& list : details)
            for (auto& comp : list)
                comp->setVisible (false);
    modeButton.setButtonText (tr (easyMode ? "ui.expand" : "ui.collapse"));
    modeButton.setTitle (modeButton.getButtonText());
    setHelpKey (modeButton, easyMode ? "ui.expand" : "ui.collapse");
    modeButton.setTooltip (shortHelp (helpKeyOf (modeButton)));
    layoutKey = -1;
}

void PluginEditor::setZoom (float z)
{
    zoom = juce::jlimit (0.75f, 1.5f, z);
    saveSetting ("zoom", zoom);
    const int w = infoOpen ? fullWidth : compactWidth;
    root.setBounds (0, 0, w, height);
    root.setTransform (juce::AffineTransform::scale (zoom));
    setSize ((int) std::lround (w * zoom), (int) std::lround (height * zoom));
    layout();
}

void PluginEditor::setInfoVisible (bool on)
{
    infoOpen = on;
    saveSetting ("info", on ? 1.0 : 0.0);
    info.setVisible (on);
    infoButton.setToggleState (on, juce::dontSendNotification);
    // Tooltips are off while the info panel shows the same text, unless asked for.
    if (! on || tooltipsWithInfo)
    {
        if (tooltips == nullptr)
            tooltips = std::make_unique<juce::TooltipWindow> (this, 700);
    }
    else
        tooltips.reset();
    setZoom (zoom);
}

void PluginEditor::selectTab (int t)
{
    tab = juce::jlimit (0, numTabs - 1, t);
    for (int i = 0; i < numTabs; ++i)
        tabs[(size_t) i].setToggleState (i == tab, juce::dontSendNotification);
    layoutKey = -1;
    layout();
}

void PluginEditor::selectGenerator (int g)
{
    selectedCard = juce::jlimit (0, numCards - 1, g);
    for (int c = 0; c < numCards; ++c)
        cards[(size_t) c]->setSelected (c == selectedCard);
    if (tab != tabDetails)
        selectTab (tabDetails);
    layoutKey = -1;
    layout();
}

void PluginEditor::hoverForTest (const juce::String& key)
{
    forcedHover = key;
    timerCallback();
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void PluginEditor::resized() {}

void PluginEditor::updateVisibility()
{
    auto show = [this] (const char* id, bool v) { if (auto* c = byId[id]) c->setVisible (v); };
    for (int c = 0; c < numCards; ++c)
        for (auto& comp : details[(size_t) c])
            comp->setVisible (! easyMode && tab == tabDetails && c == selectedCard);
    if (tab == tabDetails)
    {
        const int sel = selectedCard;
        if (sel == 0)
        {
            const bool cascade = (int) param (ids::spread_type) == 1;
            show (ids::spread_time_ms, ! cascade);
            for (const char* id : { ids::spread_density, ids::spread_f_lo, ids::spread_f_hi, ids::spread_skew, ids::spread_q })
                show (id, cascade);
        }
        if (sel == 2)
        {
            const bool chorus = (int) param (ids::mod_type) == 0;
            for (const char* id : { ids::mod_rate_hz, ids::mod_depth_ms, ids::mod_base_ms })
                show (id, chorus);
            for (const char* id : { ids::mod_cents, ids::mod_predelay_ms })
                show (id, ! chorus);
        }
        if (sel == 4)
        {
            const int mode = (int) param (ids::pan_mode);
            show (ids::pan_density, mode != 2);
            show (ids::pan_max_groups, mode == 2);
            show (ids::pan_ownership, mode != 0);
        }
        if (sel == 5)
        {
            const int mode = (int) param (ids::coh_mode);
            for (const char* id : { ids::coh_p63, ids::coh_p250, ids::coh_p1k, ids::coh_p4k, ids::coh_p16k })
                show (id, mode == 0);
            show (ids::coh_spacing_cm, mode == 1 || mode == 3);
            show (ids::coh_pattern, mode == 2 || mode == 3);
            show (ids::coh_angle_deg, mode == 2 || mode == 3);
        }
    }
    const bool full = (int) param (ids::engine) == 1;
    for (int c : { 4, 5, 8 })
        dim (*cards[(size_t) c], full);
    for (int c = 0; c < numCards; ++c)
        if (cardSource[(size_t) c] != nullptr && c != 4)
            dim (*cardSource[(size_t) c], full);
    // The analysis settings apply to the Full engine; the latency mode to both.
    for (const char* id : { ids::ambience, ids::room_decay_s, ids::transient_mode })
        dim (*byId[id], full);
    cards[8]->setVisible (stereoInput() && ! easyMode);
    // In Details, the right part shows the selected generator's own view.
    const bool detailCoh = tab == tabDetails && selectedCard == 5;
    const bool detailPan = tab == tabDetails && selectedCard == 4;
    panDisplay.setVisible (tab == tabPan || detailPan);
    coherenceView->setVisible (tab == tabCoherence || detailCoh);
    if (detailScopeTap != cardTaps[selectedCard])
    {
        detailScopeTap = cardTaps[selectedCard];
        detailScope = std::make_unique<MiniScope> (proc.core().scopes, detailScopeTap);
        detailScope->setInterceptsMouseClicks (false, false);
        centre.addAndMakeVisible (*detailScope);
    }
    detailScope->setVisible (tab == tabDetails && ! detailCoh && ! detailPan);
    if (easyMode)
        return;
    flow->setVisible (tab == tabFlow);
    scopes->setVisible (tab == tabScopes);
    bands->setVisible (tab == tabBands);
    const bool needsFull = (tab == tabPan || tab == tabCoherence) && ! full;
    centreNote.setText (tr ("ui.fullonly"), juce::dontSendNotification);
    centreNote.setVisible (needsFull && tab == tabCoherence);
}

void PluginEditor::layout()
{
    const int W = infoOpen ? fullWidth : compactWidth;
    const int gap = 6;

    // Header, 48 pt.
    {
        auto h = juce::Rectangle<int> (0, 0, W, 48).reduced (10, 10);
        h.removeFromLeft (118); // title
        modeButton.setBounds (h.removeFromLeft (96));
        h.removeFromLeft (8);
        bypass->setBounds (h.removeFromRight (64));
        h.removeFromRight (4);
        settingsButton.setBounds (h.removeFromRight (28));
        h.removeFromRight (4);
        learnButton.setBounds (h.removeFromRight (60));
        h.removeFromRight (4);
        infoButton.setBounds (h.removeFromRight (24));
        h.removeFromRight (6);
        languageSwitch->setBounds (h.removeFromRight (64));
        h.removeFromRight (8);
        copyAB.setBounds (h.removeFromRight (40));
        slotB.setBounds (h.removeFromRight (26));
        slotA.setBounds (h.removeFromRight (26));
        h.removeFromRight (6);
        redoButton.setBounds (h.removeFromRight (54));
        undoButton.setBounds (h.removeFromRight (54));
        h.removeFromRight (8);
        const bool narrow = W < fullWidth;
        latencyLabel.setVisible (true);
        latencyLabel.setBounds (h.removeFromRight (narrow ? 70 : 120));
        engine->setBounds (h.removeFromRight (104));
        engineLabel.setBounds (h.removeFromRight (narrow ? 0 : 44));
        engineLabel.setVisible (! narrow && ! easyMode);
        h.removeFromRight (8);
        nextPreset.setBounds (h.removeFromRight (22));
        prevPreset.setBounds (h.removeFromRight (22));
        h.removeFromRight (2);
        presetMenu.setBounds (h);
    }

    const int top = 54, mainH = 316, centreH = 270, bottomH = 142;
    auto left = juce::Rectangle<int> (gap, top, compactWidth - 2 * gap, mainH);

    // Meter column.
    meterPanel.setBounds (left.removeFromRight (230));
    {
        auto r = meterPanel.getLocalBounds().reduced (5);
        goniometer.setBounds (r.removeFromTop (220).withSizeKeepingCentre (220, 220));
        r.removeFromTop (3);
        auto c = r.removeFromTop (18);
        correlationLabel.setBounds (c.removeFromLeft (64));
        correlation.setBounds (c);
        r.removeFromTop (3);
        levels.setBounds (r.removeFromTop (28));
        r.removeFromTop (2);
        asw->setBounds (r);
    }
    left.removeFromRight (gap);

    // Cards.
    {
        std::vector<int> visible;
        for (int c = 0; c < numCards; ++c)
            if (c < 8 || stereoInput())
                visible.push_back (c);
        const int cw = (left.getWidth() - ((int) visible.size() - 1) * 4) / (int) visible.size();
        int x = left.getX();
        for (int c : visible)
        {
            auto& card = *cards[(size_t) c];
            card.setBounds (x, left.getY(), cw, left.getHeight());
            x += cw + 4;
            auto r = card.content();
            cardAmount[(size_t) c]->setBounds (r.removeFromTop (78));
            r.removeFromTop (6);
            if (cardSource[(size_t) c] != nullptr)
                cardSource[(size_t) c]->setBounds (r.removeFromTop (22));
            r.removeFromTop (8);
            cardScope[(size_t) c]->setBounds (r.removeFromBottom (std::min (64, r.getHeight())));
        }
    }

    // Centre strip with tabs.
    {
        auto r = juce::Rectangle<int> (gap, top + mainH + gap, compactWidth - 2 * gap, centreH);
        auto tabRow = r.removeFromTop (24);
        const int tw = 92;
        for (int t = 0; t < numTabs; ++t)
            tabs[(size_t) t].setBounds (tabRow.removeFromLeft (tw));
        r.removeFromTop (4);
        centre.setBounds (r);
        auto inner = centre.getLocalBounds().reduced (8);
        const bool detailView = tab == tabDetails;
        auto viewArea = detailView ? inner.withTrimmedLeft (inner.getWidth() / 2 + 40) : inner;
        panDisplay.setBounds (viewArea);
        coherenceView->setBounds (viewArea);
        if (detailScope != nullptr)
            detailScope->setBounds (inner.withTrimmedLeft (inner.getWidth() / 2 + 40));
        flow->setBounds (inner);
        scopes->setBounds (inner);
        bands->setBounds (inner);
        centreNote.setBounds (inner.removeFromTop (20));

        // Details of the selected generator: choices in a column, knobs in a grid.
        auto d = r.reduced (10, 8).withWidth (r.getWidth() / 2 + 30);
        auto choiceCol = d.removeFromLeft (200);
        int kx = d.getX(), ky = d.getY();
        const int kw = 70, kh = 76;
        for (auto& comp : details[(size_t) selectedCard])
        {
            if (! comp->isVisible() && tab == tabDetails)
                continue;
            if (dynamic_cast<Choice*> (comp.get()) != nullptr)
                comp->setBounds (choiceCol.removeFromTop (24).withTrimmedBottom (2));
            else
            {
                if (kx + kw > d.getRight())
                {
                    kx = d.getX();
                    ky += kh + 4;
                }
                comp->setBounds (kx, ky, kw, kh);
                kx += kw + 4;
            }
        }
    }

    // Bottom row.
    {
        auto r = juce::Rectangle<int> (gap, top + mainH + gap + centreH + gap, compactWidth - 2 * gap, bottomH);
        analysisPanel.setBounds (r.removeFromLeft (262));
        r.removeFromLeft (gap);
        outputPanel.setBounds (r.removeFromRight (318));
        r.removeFromRight (gap);
        sidePanel.setBounds (r);
        {
            auto a = analysisPanel.content();
            byId[ids::ambience]->setBounds (a.removeFromLeft (56));
            byId[ids::room_decay_s]->setBounds (a.removeFromLeft (56));
            a.removeFromLeft (4);
            a.removeFromTop (14);
            byId[ids::transient_mode]->setBounds (a.removeFromTop (24).withTrimmedBottom (2));
            a.removeFromTop (8);
            byId[ids::latency_mode]->setBounds (a.removeFromTop (24).withTrimmedBottom (2));
        }
        {
            auto sb = sidePanel.content();
            auto g = sb.removeFromRight (62);
            const int w = sb.getWidth() / 7;
            for (const char* id : { ids::bass_mono_hz, ids::band_xover_lo, ids::band_xover_hi, ids::band_low, ids::band_mid, ids::band_high, ids::transient_duck })
                byId[id]->setBounds (sb.removeFromLeft (w));
            guardLabel.setBounds (g.removeFromTop (30).withTrimmedTop (4));
            byId[ids::guard]->setBounds (g.removeFromTop (24).reduced (4, 0));
            g.removeFromTop (6);
            guardCeiling.setBounds (g.removeFromTop (20).reduced (2, 0));
        }
        {
            auto o = outputPanel.content();
            byId[ids::width]->setBounds (o.removeFromLeft (74));
            byId[ids::mid_blend]->setBounds (o.removeFromLeft (58));
            byId[ids::out_gain_db]->setBounds (o.removeFromLeft (52));
            o.removeFromLeft (4);
            monoWarning.setBounds (o.removeFromTop (14));
            byId[ids::comp_mode]->setBounds (o.removeFromTop (24).withTrimmedBottom (2));
            auto autoRow = o.removeFromTop (24).withTrimmedBottom (2);
            byId[ids::width_mode]->setBounds (autoRow.removeFromLeft (autoRow.getWidth() * 2 / 3));
            aswTarget.setBounds (autoRow.withTrimmedLeft (4));
            o.removeFromTop (4);
            byId[ids::listen]->setBounds (o.removeFromTop (24));
        }
    }

    info.setBounds (compactWidth, top, fullWidth - compactWidth - gap, height - top - gap);
    info.setVisible (infoOpen);
    easy->setBounds (gap, top, compactWidth - 2 * gap, height - top - gap);
    overlay.setBounds (root.getLocalBounds());
    confirm.setBounds (root.getLocalBounds());
}

juce::String PluginEditor::liveText (const juce::String& key) const
{
    // Live explanations (PART2_LEDGER.md L4), computed from current values.
    auto value = [this] (const char* id)
    {
        auto* p = proc.state().getParameter (id);
        if (auto* f = dynamic_cast<ExactFloatParameter*> (p))
            return (double) f->plain();
        return (double) p->convertFrom0to1 (p->getValue());
    };
    if (key == "p.delay_time_ms")
    {
        const double t = value (ids::delay_time_ms);
        return cz() ? U ("Při ") + fmtN (t, 1) + U (" ms jsou zářezy hřebene od sebe ") + fmtN (1000.0 / t, 1) + U (" Hz; první je na ") + fmtN (500.0 / t, 1) + " Hz."
                    : "At " + fmtN (t, 1) + " ms the comb notches are " + fmtN (1000.0 / t, 1) + " Hz apart; the first is at " + fmtN (500.0 / t, 1) + " Hz.";
    }
    if (key == "p.spread_time_ms")
    {
        const double t = value (ids::spread_time_ms);
        return cz() ? U ("Levý a pravý kanál tvoří doplňkové hřebeny se zuby po ") + fmtN (1000.0 / t, 1) + " Hz."
                    : "Left and right are complementary combs with teeth every " + fmtN (1000.0 / t, 1) + " Hz.";
    }
    if (key == "p.width")
    {
        const double w = value (ids::width) / 100.0;
        const auto db = w > 0.0 ? fmtN (20.0 * std::log10 (w), 1) + " dB" : juce::String ("-inf dB");
        return cz() ? U ("Syntetizovaná strana je ") + db + " proti 100 %." : "Synthesised side is " + db + " relative to 100 %.";
    }
    if (key == "p.mod_cents")
    {
        const double c = value (ids::mod_cents);
        return cz() ? U ("Levý kanál je transponován na ") + fmtN (std::pow (2.0, c / 1200.0), 4) + U ("násobek, pravý na ") + fmtN (std::pow (2.0, -c / 1200.0), 4) + ": rozestup " + fmtN (2.0 * c, 1) + U (" centů.")
                    : "Left is shifted to " + fmtN (std::pow (2.0, c / 1200.0), 4) + " times the pitch, right to " + fmtN (std::pow (2.0, -c / 1200.0), 4) + ": a " + fmtN (2.0 * c, 1) + "-cent spread.";
    }
    if (key == "p.bass_mono_hz")
    {
        const double f = value (ids::bass_mono_hz);
        if (f <= 20.0)
            return cz() ? U ("Filtr je vypnutý: i basy mohou být široké.") : "The filter is off: the bass can be wide too.";
        return cz() ? "Pod " + fmtN (f, 0) + U (" Hz je strana tlumena strmostí 24 dB/okt.; basy zůstávají uprostřed.")
                    : "Below " + fmtN (f, 0) + " Hz the side is rolled off at 24 dB/octave; bass stays centred.";
    }
    if (key == "p.engine" || key == "p.latency_mode")
    {
        const int n = proc.getLatencySamples();
        const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        if (n == 0)
            return cz() ? U ("Latence je nulová.") : "Latency is zero.";
        return cz() ? "Latence " + juce::String (n) + U (" vzorků (") + fmtN (1000.0 * n / fs, 1) + U (" ms); hostitel ji vyrovná.")
                    : "Latency is " + juce::String (n) + " samples (" + fmtN (1000.0 * n / fs, 1) + " ms); your host compensates for it.";
    }
    if (key == "p.mid_blend")
    {
        if (value (ids::mid_blend) <= 0.0)
            return cz() ? U ("Mono součet se přesně rovná vstupu.") : "The mono sum equals the input exactly.";
        const auto d = fmtN (analysis.worstMonoFoldDb(), 1);
        return cz() ? U ("Mono součet se nyní odchyluje od vstupu až o ") + d + U (" dB (měřeno živě).") : "The mono sum now deviates by up to " + d + " dB from the input (measured live).";
    }
    if (key.startsWith ("p.coh_") || key == "card.coherence")
    {
        const auto& c = proc.core().coherence();
        int band = 0;
        const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        for (int b = 0; b < CoherenceDesigner::numBands; ++b)
            if (std::abs (std::log (CoherenceDesigner::bandCentre (b, fs) / 1000.0)) < std::abs (std::log (CoherenceDesigner::bandCentre (band, fs) / 1000.0)))
                band = b;
        return cz() ? U ("Cílová koherence na 1 kHz je ") + fmtN (c.targetCoherence (band), 2) + U ("; dosaženo ") + fmtN (c.achievedCoherence (band), 2) + "."
                    : "Target coherence at 1 kHz is " + fmtN (c.targetCoherence (band), 2) + "; achieved " + fmtN (c.achievedCoherence (band), 2) + ".";
    }
    if (key == "p.room_size")
    {
        const double sz = value (ids::room_size);
        const double first = (0.8 * sz * 0.9 - value (ids::room_distance)) / 343.0 * 1000.0;
        return cz() ? U ("Místnost ") + fmtN (sz, 1) + " x " + fmtN (0.8 * sz, 1) + U (" m; odrazy přicházejí zhruba od ") + fmtN (std::max (1.0, first * 0.25), 0) + " ms."
                    : "Room " + fmtN (sz, 1) + " x " + fmtN (0.8 * sz, 1) + " m; reflections arrive from about " + fmtN (std::max (1.0, first * 0.25), 0) + " ms.";
    }
    if (key == "p.coh_spacing_cm" || key == "p.dbl_offset_ms")
        return {};
    return {};
}

bool PluginEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
        return proc.undo();
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
        return proc.redo();
    return false;
}

void PluginEditor::timerCallback()
{
    if (proc.uiLanguage() != appliedLanguage)
        applyLanguage();
    if (proc.isEasyMode() != easyMode)
        applyMode();

    const int key = (int) param (ids::spread_type) | ((int) param (ids::mod_type) << 1) | ((int) param (ids::pan_mode) << 2)
                    | ((int) param (ids::engine) << 4) | ((int) param (ids::coh_mode) << 5) | (stereoInput() ? 1 << 7 : 0)
                    | ((int) param (ids::latency_mode) << 8) | (tab << 9) | (selectedCard << 12) | (easyMode ? 1 << 16 : 0);
    if (key != layoutKey)
    {
        layoutKey = key;
        updateVisibility();
        layout();
    }

    if (proc.getCurrentProgram() != lastProgram)
    {
        lastProgram = proc.getCurrentProgram();
        presetMenu.setSelectedId (lastProgram + 1, juce::dontSendNotification);
    }

    const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    latencyLabel.setText (tr ("ui.latency") + ": " + fmtN (1000.0 * proc.getLatencySamples() / fs, 1) + " ms", juce::dontSendNotification);
    monoWarning.setVisible (param (ids::mid_blend) > 0.0f);
    aswTarget.setVisible ((int) param (ids::width_mode) == 1);
    aswTarget.setTitle (tr ("cap.asw_target"));
    undoButton.setEnabled (proc.canUndo());
    redoButton.setEnabled (proc.canRedo());
    slotA.setToggleState (proc.compareSlot() == 0, juce::dontSendNotification);
    slotB.setToggleState (proc.compareSlot() == 1, juce::dontSendNotification);

    // An undo point after every finished gesture.
    if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        proc.commitUndoPoint();

    auto& core = proc.core();
    for (int g = 0; g < 8; ++g)
    {
        const bool awake = core.sleepController ((GeneratorId) g).awakeFlag.load (std::memory_order_relaxed);
        cards[(size_t) g]->setAwake (awake);
        cardScope[(size_t) g]->dim = ! awake;
    }
    cards[8]->setAwake (core.expanderActive());

    const double now = juce::Time::getMillisecondCounterHiRes();
    const int n = core.meters.popGoniometer (gonioBuffer.data(), Meters::fifoPairs);
    if (easyMode)
        easy->refresh (gonioBuffer.data(), n, now);
    else
    {
        goniometer.push (gonioBuffer.data(), n, now);
        goniometer.prune (now);
        goniometer.repaint();
    }
    correlation.setValue (core.meters.correlation());
    levels.setPeaks (core.meters.takePeak (0), core.meters.takePeak (1), now);
    analysis.pull (core.outputRing);
    asw->repaint();
    for (auto& s : cardScope)
        s->repaint();
    if (panDisplay.isVisible())
        panDisplay.update (core.panMap(), (int) param (ids::engine) == 1);
    if (coherenceView->isVisible())
        coherenceView->repaint();
    if (detailScope != nullptr && detailScope->isVisible())
        detailScope->repaint();
    if (tab == tabFlow)
        flow->repaint();
    if (tab == tabScopes)
        scopes->refresh();
    if (tab == tabBands)
        bands->repaint();

    // Info panel: the entry under the mouse.
    juce::String hk = forcedHover;
    if (hk.isEmpty())
    {
        const auto pos = root.getMouseXYRelative();
        if (auto* c = root.getComponentAt (pos))
            for (auto* p = c; p != nullptr && p != &root; p = p->getParentComponent())
                if (helpKeyOf (*p).isNotEmpty())
                {
                    hk = helpKeyOf (*p);
                    break;
                }
        if (hk.isEmpty())
            hk = easyMode ? juce::String ("ui.easy") : "preset." + juce::String (proc.getCurrentProgram() + 1);
    }
    if (hk == "disp.flow" && flow->hoveredHelp().isNotEmpty())
        hk = flow->hoveredHelp();
    info.show (hk, liveText (hk));

    // Tour highlight.
    overlay.boxes.clear();
    if (auto* step = info.currentStep())
    {
        std::function<void (juce::Component&)> visit = [&] (juce::Component& c)
        {
            if (c.isShowing())
                for (const char* h : step->highlight)
                    if (helpKeyOf (c) == h && dynamic_cast<juce::Slider*> (&c) == nullptr && dynamic_cast<juce::ComboBox*> (&c) == nullptr)
                        overlay.boxes.push_back (root.getLocalArea (c.getParentComponent(), c.getBounds()));
            for (auto* ch : c.getChildren())
                visit (*ch);
        };
        visit (root);
    }
    overlay.repaint();
}
} // namespace sph
