#pragma once

#include "ui/Widgets.h"

#include <functional>
#include <vector>

namespace sph::ui
{
// A lesson of the Learn menu: steps of text, each highlighting controls by
// help key, optionally applying a demonstration state (LearnContent.cpp).
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

// The reserved info area: title, what it does, a live explanation, how it
// works, a try-this, an in-depth section and references; or a lesson.
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

    // Long entries scroll with the mouse wheel; the offset resets for each
    // new entry or tour step.
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void scrollBy (float points);
    float scrollOffset() const noexcept { return scroll; }
    float maxScroll() const;
    // Lays out the current text and returns its height (tests).
    float measure();

private:
    juce::AttributedString compose() const;
    juce::String key, live;
    juce::TextButton pin, back, next, apply, close;
    juce::OwnedArray<juce::TextButton> tourButtons;
    int tour = -1, step = 0;
    bool listing = false;
    float scroll = 0.0f, contentHeight = 0.0f;
    juce::Rectangle<int> textArea() const;
};
} // namespace sph::ui
