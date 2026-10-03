#pragma once

#include "dsp/Params.h"
#include "dsp/ScopeTaps.h"
#include "dsp/StereoBands.h"
#include "dsp/VirtualListener.h"
#include "ui/Widgets.h"

#include <functional>

namespace sph
{
class CoherenceDesigner;
class StereophieldProcessor;
} // namespace sph

namespace sph::ui
{
// Draws min/max columns of one scope tap over a time span (V1, V4).
void drawTap (juce::Graphics& g, juce::Rectangle<float> r, const ScopeTaps& taps, int tap, double seconds,
              int64_t endColumn, juce::Colour colour, bool unipolar = false);

// Mini waveform of a generator's side, under its card (V4).
class MiniScope : public juce::Component
{
public:
    MiniScope (const ScopeTaps& t, int tapIndex) : taps (t), tap (tapIndex) {}
    void paint (juce::Graphics&) override;
    bool dim = false;

private:
    const ScopeTaps& taps;
    int tap;
};

// The frequency-domain meters' feed: pops the output ring and runs the
// per-band correlation, mono-fold and virtual-listener analyses (V6, V7).
class OutputAnalysis
{
public:
    void prepare (double sampleRate);
    void pull (OutputRing& ring);
    void setPlayback (VirtualListener::Playback p);
    VirtualListener::Playback playback() const noexcept { return mode; }
    const StereoBands& bands() const noexcept { return sb; }
    const VirtualListener& listener() const noexcept { return vl; }
    float worstMonoFoldDb() const noexcept;

private:
    double fs = 48000.0;
    StereoBands sb;
    VirtualListener vl;
    VirtualListener::Playback mode = VirtualListener::Playback::Speakers;
    std::vector<float> l, r, m, tl, tr, tm;
    int fill = 0, sinceBands = 0, sinceListener = 0;
};

// Stacked scope lanes with a picker per lane, a shared span and freeze (V3, V5).
class ScopeLanes : public juce::Component, public Localisable
{
public:
    static constexpr int numLanes = 6;
    static constexpr int activityTap = ScopeTaps::numTaps; // pseudo-tap: activity timeline

    explicit ScopeLanes (const ScopeTaps& t);
    void resized() override;
    void paint (juce::Graphics&) override;
    void localise() override;
    void showTap (int tap); // puts a tap in the first lane
    void refresh() { if (! frozen.getToggleState()) end = taps.columnsWritten (ScopeTaps::outL); repaint(); }

private:
    juce::String laneName (int tap) const;
    const ScopeTaps& taps;
    std::array<juce::ComboBox, numLanes> pickers;
    juce::ComboBox span;
    juce::TextButton frozen;
    int64_t end = 0;
};

// The whole plugin as a live signal-flow diagram (V2).
class FlowView : public juce::Component, public Localisable
{
public:
    FlowView (const ScopeTaps& t, std::function<bool (int generator)> awake);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void localise() override { repaint(); }
    std::function<void (int tap)> onOpenTap;
    juce::String hoveredHelp() const { return hoverHelp; }

private:
    struct Node
    {
        juce::String titleKey, helpKey;
        int tap, generator;
        juce::Rectangle<float> box;
    };
    std::vector<Node> layoutNodes() const;
    const ScopeTaps& taps;
    std::function<bool (int)> isAwake;
    juce::String hoverHelp;
};

// Per-band correlation and mono-fold deviation (V6).
class BandsView : public juce::Component
{
public:
    explicit BandsView (const OutputAnalysis& a) : analysis (a) {}
    void paint (juce::Graphics&) override;

private:
    const OutputAnalysis& analysis;
};

// Target and achieved coherence per band, with draggable curve points (G6).
class CoherenceView : public juce::Component
{
public:
    CoherenceView (APVTS& state, const CoherenceDesigner& designer, double sampleRate);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setSampleRate (double fs) { rate = fs; }

private:
    juce::Rectangle<float> plot() const;
    Params currentParams() const;
    float xOf (double f) const;
    float yOf (double c) const;
    APVTS& state;
    const CoherenceDesigner& coh;
    double rate;
    int dragging = -1;
    std::array<std::unique_ptr<juce::ParameterAttachment>, 5> points;
    std::array<float, 5> values {};
};

// Perceived-width bar with Speakers / Headphones (V7).
class AswMeter : public juce::Component, public Localisable
{
public:
    explicit AswMeter (OutputAnalysis& a);
    void resized() override;
    void paint (juce::Graphics&) override;
    void localise() override { mode.localise(); repaint(); }

private:
    OutputAnalysis& analysis;
    Segmented mode;
};
} // namespace sph::ui
