#pragma once

#include "ui/Displays.h"
#include "ui/Style.h"
#include "ui/Widgets.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

namespace sph
{
class StereophieldProcessor;

// The editor of DESIGN.md section 9: one fixed 980 x 640 window.
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit PluginEditor (StereophieldProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Runs one display refresh now; used by the snapshot test.
    void refreshForTest() { timerCallback(); }

    static constexpr int width = 980, height = 640;

private:
    void timerCallback() override;
    void updateVisibility();
    float param (const char* id) const;

    StereophieldProcessor& proc;
    ui::LookAndFeel lnf;

    // Header.
    juce::ComboBox presetMenu;
    juce::Label engineLabel, latencyLabel;
    std::unique_ptr<ui::Segmented> engine;
    std::unique_ptr<ui::Toggle> bypass;

    // Generator cards.
    ui::Panel spreadCard { "SPREAD" }, delayCard { "DELAY" }, modCard { "MOD" }, velvetCard { "VELVET" }, panCard { "PAN MAP" };
    std::unique_ptr<ui::Knob> spreadAmount, spreadTime, spreadDensity, spreadLo, spreadHi, spreadSkew, spreadQ;
    std::unique_ptr<ui::Choice> spreadSource, spreadType;
    std::unique_ptr<ui::Knob> delayAmount, delayTime, delayLp;
    std::unique_ptr<ui::Choice> delaySource, delaySide;
    std::unique_ptr<ui::Knob> modAmount, modRate, modDepth, modBase, modCents, modPredelay;
    std::unique_ptr<ui::Choice> modSource, modType;
    std::unique_ptr<ui::Knob> velvetAmount, velvetSize, velvetDensity, velvetVariation;
    std::unique_ptr<ui::Choice> velvetSource;
    std::unique_ptr<ui::Knob> panAmount, panDepth, panDensity, panBass, panGroups;
    std::unique_ptr<ui::Choice> panMode;

    // Meters.
    ui::Panel meterPanel { "" };
    ui::Goniometer goniometer;
    ui::CorrelationMeter correlation;
    ui::LevelMeters levels;
    juce::Label correlationLabel;

    // Pan map display.
    ui::Panel panDisplayPanel { "PAN MAP DISPLAY" };
    ui::PanMapDisplay panDisplay;

    // Bottom row.
    ui::Panel analysisPanel { "ANALYSIS" }, sidePanel { "SIDE BUS" }, outputPanel { "OUTPUT" };
    std::unique_ptr<ui::Knob> ambience, roomDecay;
    std::unique_ptr<ui::Knob> bassMono, xoverLo, xoverHi, bandLow, bandMid, bandHigh, duck;
    std::unique_ptr<ui::Toggle> guard;
    juce::Label guardLabel;
    std::unique_ptr<ui::Knob> widthKnob, midBlend, outGain;
    std::unique_ptr<ui::Choice> compMode;
    std::unique_ptr<ui::Segmented> listen;
    juce::Label monoWarning;

    std::vector<float> gonioBuffer;
    int lastProgram = -1;
    int layoutKey = -1;
};
} // namespace sph
