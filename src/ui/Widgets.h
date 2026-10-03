#pragma once

#include "ui/Strings.h"
#include "ui/Style.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

namespace sph::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// Every widget carries a help key (its info-panel entry) as the component
// property "help", and re-reads its texts in localise().
void setHelpKey (juce::Component& c, const juce::String& key);
juce::String helpKeyOf (const juce::Component& c);
// First sentence of a help entry, for tooltips.
juce::String shortHelp (const juce::String& key);

class Localisable
{
public:
    virtual ~Localisable() = default;
    virtual void localise() = 0;
};

// A rotary knob with its caption above and its value (with units) below.
class Knob : public juce::Component, public Localisable
{
public:
    Knob (APVTS& state, const juce::String& paramId, bool large = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    void localise() override;
    juce::Slider& slider() noexcept { return knob; }

private:
    juce::Slider knob;
    juce::String id;
    bool big;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    std::function<juce::String (double)> baseText;
};

// A combo box with a caption, bound to a choice parameter.
class Choice : public juce::Component, public Localisable
{
public:
    Choice (APVTS& state, const juce::String& paramId, bool showCaption = true);
    void resized() override;
    void paint (juce::Graphics&) override;
    void localise() override;
    juce::ComboBox& box() noexcept { return combo; }

private:
    juce::String itemKey (int index) const;
    juce::ComboBox combo;
    juce::String id;
    bool caption;
    int items = 0;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// Connected buttons selecting one item of a choice parameter, or (with no
// parameter) calling onSelect.
class Segmented : public juce::Component, public Localisable
{
public:
    Segmented (APVTS* state, const juce::String& paramId, std::vector<juce::String> labelKeys);
    ~Segmented() override;
    void resized() override;
    void localise() override;
    void select (int index);
    int selected() const noexcept { return current; }
    std::function<void (int)> onSelect;

private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::vector<juce::String> keys;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int current = 0;
};

// A toggle button bound to a two-state parameter.
class Toggle : public juce::Component, public Localisable
{
public:
    Toggle (APVTS& state, const juce::String& paramId, const juce::String& labelKey);
    void resized() override { button.setBounds (getLocalBounds()); }
    void localise() override;
    juce::TextButton& get() noexcept { return button; }

private:
    juce::TextButton button;
    juce::String key;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// A rounded panel with a title and an optional awake indicator.
class Panel : public juce::Component, public Localisable
{
public:
    explicit Panel (const juce::String& titleKey);
    void paint (juce::Graphics&) override;
    void localise() override { repaint(); }
    juce::Rectangle<int> content() const;
    void showIndicator (bool show) { indicator = show; }
    void setAwake (bool awake);
    void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }
    std::function<void()> onClick;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    juce::String key;
    bool indicator = false, awake = true, selected = false;
};

// Calls localise() on every Localisable in a component tree.
void localiseTree (juce::Component& root);
} // namespace sph::ui
