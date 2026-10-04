#pragma once

#include "ui/Displays.h"
#include "ui/EasyView.h"
#include "ui/InfoPanel.h"
#include "ui/Style.h"
#include "ui/Views.h"
#include "ui/Widgets.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>
#include <memory>
#include <vector>

namespace sph
{
class StereophieldProcessor;
using APVTSAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

// The editor: 1240 x 800 points with the
// info panel open, 980 x 800 with it collapsed, scaled by the zoom setting.
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer,
                     private juce::ChangeListener
{
public:
    explicit PluginEditor (StereophieldProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int fullWidth = 1240, compactWidth = 980, height = 800;

    // Test hooks.
    void refreshForTest() { timerCallback(); }
    void selectTab (int tab);
    void selectGenerator (int g);
    void setInfoVisible (bool on);
    void setZoom (float z);
    void setLanguageForTest (int l);
    void hoverForTest (const juce::String& helpKey);
    ui::InfoPanel& infoPanel() noexcept { return info; }
    juce::Component& rootComponent() noexcept { return root; }
    juce::String liveText (const juce::String& helpKey) const;
    // Easy mode: the header's mode button expands
    // at once, or asks before collapsing. answerCollapseForTest presses the
    // confirmation's OK (true) or Cancel (false).
    void pressModeButtonForTest() { modeButton.onClick(); }
    bool collapseQuestionShowing() const { return confirm.isVisible(); }
    void answerCollapseForTest (bool ok) { (ok ? confirm.ok : confirm.cancel).onClick(); }
    bool easyShowing() const { return easy != nullptr && easy->isVisible(); }

    enum Tab { tabDetails = 0, tabPan, tabCoherence, tabFlow, tabScopes, tabBands, numTabs };

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void layout();
    void updateVisibility();
    void applyMode();
    void askCollapse();
    void applyLanguage();
    void buildPresetMenu();
    void showSettingsMenu();
    void savePresetDialog();
    float param (const char* id) const;
    bool stereoInput() const;

    StereophieldProcessor& proc;
    ui::LookAndFeel lnf;
    struct Root : juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            g.fillAll (ui::colours::background);
            g.setColour (ui::colours::text);
            g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
            g.drawText ("stereophield", juce::Rectangle<int> (12, 0, 118, 48), juce::Justification::centredLeft);
        }
    } root;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    float zoom = 1.0f;
    bool infoOpen = true, tooltipsWithInfo = false;

    // Header.
    juce::ComboBox presetMenu;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, undoButton, redoButton, slotA { "A" }, slotB { "B" },
        copyAB, infoButton, learnButton, settingsButton { "..." };
    juce::Label engineLabel, latencyLabel;
    std::unique_ptr<ui::Segmented> engine, languageSwitch;
    std::unique_ptr<ui::Toggle> bypass;

    // Generator cards.
    static constexpr int numCards = 9;
    std::array<std::unique_ptr<ui::Panel>, numCards> cards;
    std::array<std::unique_ptr<ui::Knob>, numCards> cardAmount;
    std::array<std::unique_ptr<ui::Choice>, numCards> cardSource;
    std::array<std::unique_ptr<ui::MiniScope>, numCards> cardScope;
    int selectedCard = 0;

    // Details: every generator's controls, shown for the selected card.
    std::array<std::vector<std::unique_ptr<juce::Component>>, numCards> details;
    std::map<juce::String, juce::Component*> byId;

    // Meters.
    ui::Panel meterPanel { "" };
    ui::Goniometer goniometer;
    ui::CorrelationMeter correlation;
    ui::LevelMeters levels;
    juce::Label correlationLabel;
    ui::OutputAnalysis analysis;
    std::unique_ptr<ui::AswMeter> asw;

    // Centre strip.
    std::array<juce::TextButton, numTabs> tabs;
    int tab = tabDetails;
    ui::Panel centre { "" };
    ui::PanMapDisplay panDisplay;
    std::unique_ptr<ui::CoherenceView> coherenceView;
    std::unique_ptr<ui::FlowView> flow;
    std::unique_ptr<ui::ScopeLanes> scopes;
    std::unique_ptr<ui::BandsView> bands;
    juce::Label centreNote;
    std::unique_ptr<ui::MiniScope> detailScope; // large waveform of the selected generator
    int detailScopeTap = -1;

    // Bottom row.
    ui::Panel analysisPanel { "panel.analysis" }, sidePanel { "panel.side" }, outputPanel { "panel.output" };
    std::vector<std::unique_ptr<juce::Component>> bottomControls;
    juce::Label guardLabel, monoWarning;
    juce::Slider aswTarget; // compact bar beside the width mode
    std::unique_ptr<APVTSAttachment> aswAttachment;

    ui::InfoPanel info;

    // Easy mode.
    std::unique_ptr<ui::EasyView> easy;
    juce::TextButton modeButton;
    juce::Slider guardCeiling; // compact bar under the guard toggle
    std::unique_ptr<APVTSAttachment> guardCeilingAttachment;
    bool easyMode = false;

    // The question before collapsing to Easy mode: a sheet over the editor.
    struct Confirm : juce::Component
    {
        juce::Label title, body;
        juce::TextButton ok, cancel;
        Confirm();
        void paint (juce::Graphics&) override;
        void resized() override;
    } confirm;

    // Tour highlight overlay.
    struct Overlay : juce::Component
    {
        std::vector<juce::Rectangle<int>> boxes;
        void paint (juce::Graphics& g) override
        {
            g.setColour (ui::colours::accent);
            for (auto& b : boxes)
                g.drawRoundedRectangle (b.toFloat().expanded (2.0f), 6.0f, 2.0f);
        }
    } overlay;

    std::vector<float> gonioBuffer;
    int lastProgram = -1, layoutKey = -1, appliedLanguage = -1;
    juce::String hoverKey, forcedHover;
};
} // namespace sph
