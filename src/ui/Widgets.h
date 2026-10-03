#pragma once

#include "ui/Style.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

namespace sph::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// A rotary knob with its name above and the value (with units) below, bound
// to one parameter. Double-click returns to the default.
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& name, bool large = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider& slider() noexcept { return knob; }

private:
    juce::Slider knob;
    juce::String title;
    bool big;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// A combo box with a small caption, bound to a choice parameter.
class Choice : public juce::Component
{
public:
    Choice (APVTS& state, const juce::String& paramId, const juce::String& caption);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::ComboBox& box() noexcept { return combo; }

private:
    juce::ComboBox combo;
    juce::String caption;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// A row of connected buttons that selects one item of a choice parameter.
class Segmented : public juce::Component
{
public:
    Segmented (APVTS& state, const juce::String& paramId, const juce::StringArray& labels);
    ~Segmented() override;
    void resized() override;

private:
    void select (int index);
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::RangedAudioParameter* param = nullptr;
};

// A toggle button bound to a two-state parameter.
class Toggle : public juce::Component
{
public:
    Toggle (APVTS& state, const juce::String& paramId, const juce::String& text);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::TextButton& get() noexcept { return button; }

private:
    juce::TextButton button;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// A rounded panel with a title.
class Panel : public juce::Component
{
public:
    explicit Panel (const juce::String& title);
    void paint (juce::Graphics&) override;
    // Area below the title.
    juce::Rectangle<int> content() const;
    // A small status dot in the title row: lit when awake, dim when asleep.
    void showIndicator (bool show) { indicator = show; }
    void setAwake (bool awake);

private:
    juce::String title;
    bool indicator = false, awake = true;
};
} // namespace sph::ui
