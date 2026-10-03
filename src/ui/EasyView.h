#pragma once

#include "plugin/EasyMode.h"
#include "ui/Displays.h"
#include "ui/Views.h"
#include "ui/Widgets.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace sph
{
class StereophieldProcessor;
}

namespace sph::ui
{
// The Easy mode face (PART3_LEDGER.md section 2): four macros, Adapt and Low
// latency, the meters, the detected material and what the macros are doing
// under the hood. Fills the area left of the info panel, below the header.
class EasyView : public juce::Component, public Localisable
{
public:
    EasyView (StereophieldProcessor& p, OutputAnalysis& analysis);
    ~EasyView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void localise() override;

    // From the editor's timer.
    void refresh (const float* gonioPairs, int numPairs, double nowMs);

private:
    StereophieldProcessor& proc;

    Panel macroPanel { "panel.easy" }, meterPanel { "panel.listen" }, hoodPanel { "panel.hood" };
    Knob width, character, space, focus;
    Toggle adapt, lowLatency;
    juce::Label tagline;

    Goniometer goniometer;
    CorrelationMeter correlation;
    juce::Label correlationLabel;
    std::unique_ptr<AswMeter> asw;

    // The material weights, three bars.
    struct Material : juce::Component
    {
        easy::Weights w { 0, 0, 1 };
        bool adaptive = true;
        void paint (juce::Graphics&) override;
    } material;

    // One internal parameter the macros drive: caption, value and a bar.
    struct HoodRow : juce::Component
    {
        juce::String id;
        float norm = 0.0f;
        juce::String text;
        bool active = true;
        void paint (juce::Graphics&) override;
    };
    std::vector<std::unique_ptr<HoodRow>> rows;
};
} // namespace sph::ui
