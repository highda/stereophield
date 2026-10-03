#pragma once

#include "ui/Widgets.h"

#include <functional>
#include <vector>

namespace sph::ui
{
// A guided tour (PART2_LEDGER.md L5): steps of text, each highlighting
// controls by help key, optionally applying a demonstration state.
struct TourStep
{
    const char* en;
    const char* cs;
    std::vector<const char*> highlight;                    // help keys
    std::vector<std::pair<const char*, float>> apply;      // parameter changes, empty for none
};

struct Tour
{
    const char* titleEn;
    const char* titleCs;
    std::vector<TourStep> steps;
};

const std::vector<Tour>& tours();

// The reserved info area (L2): title, what it does, a live explanation, how
// it works, a try-this, and a reference; or a guided tour.
class InfoPanel : public juce::Component, public Localisable
{
public:
    InfoPanel();
    void paint (juce::Graphics&) override;
    void resized() override;
    void localise() override;

    // Shows an entry unless pinned or a tour is running.
    void show (const juce::String& helpKey, const juce::String& liveText);
    juce::String currentKey() const { return key; }
    bool isPinned() const { return pin.getToggleState(); }

    // Tours.
    void openTourList();
    void startTour (int index);
    bool inTour() const { return tour >= 0; }
    const TourStep* currentStep() const;
    std::function<void (const TourStep&)> onApply;
    std::function<void()> onTourChanged;

    // For tests: the text height an entry needs at the panel's width.
    static float textHeight (const juce::String& helpKey, float width);

private:
    juce::AttributedString compose() const;
    juce::String key, live;
    juce::TextButton pin, back, next, apply, close;
    juce::OwnedArray<juce::TextButton> tourButtons;
    int tour = -1, step = 0;
    bool listing = false;
};
} // namespace sph::ui
