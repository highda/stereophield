#include "ui/Widgets.h"

namespace sph::ui
{
Knob::Knob (APVTS& state, const juce::String& paramId, const juce::String& name, bool large)
    : title (name), big (large)
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 14);
    knob.setColour (juce::Slider::textBoxTextColourId, colours::secondary);
    addAndMakeVisible (knob);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, knob);
    if (auto* p = state.getParameter (paramId))
        knob.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    knob.setTitle (name);
    setTitle (name);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (14);
    knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, getWidth(), 14);
    knob.setBounds (r);
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (isEnabled() ? colours::text : colours::secondary);
    g.setFont (big ? bodyFont() : labelFont());
    g.drawFittedText (title, getLocalBounds().removeFromTop (14), juce::Justification::centred, 1);
}

Choice::Choice (APVTS& state, const juce::String& paramId, const juce::String& cap) : caption (cap)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        combo.addItemList (p->choices, 1);
    addAndMakeVisible (combo);
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramId, combo);
    combo.setTitle (cap);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    if (caption.isNotEmpty())
        r.removeFromLeft (juce::jmin (44, r.getWidth() / 3));
    combo.setBounds (r);
}

void Choice::paint (juce::Graphics& g)
{
    if (caption.isEmpty())
        return;
    g.setColour (colours::secondary);
    g.setFont (labelFont());
    g.drawFittedText (caption, getLocalBounds().removeFromLeft (juce::jmin (44, getWidth() / 3)),
                      juce::Justification::centredLeft, 1);
}

Segmented::Segmented (APVTS& state, const juce::String& paramId, const juce::StringArray& labels)
{
    param = state.getParameter (paramId);
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setClickingTogglesState (false);
        b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0)
                              | (i + 1 < labels.size() ? juce::Button::ConnectedOnRight : 0));
        b->onClick = [this, i] { if (attachment != nullptr) attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    if (param != nullptr)
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { select ((int) std::lround (v)); });
    if (attachment != nullptr)
        attachment->sendInitialUpdate();
}

Segmented::~Segmented() = default;

void Segmented::select (int index)
{
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == index, juce::dontSendNotification);
}

void Segmented::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / juce::jmax (1, buttons.size());
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds (i + 1 == buttons.size() ? r : r.removeFromLeft (w));
}

Toggle::Toggle (APVTS& state, const juce::String& paramId, const juce::String& text)
{
    button.setButtonText (text);
    button.setClickingTogglesState (true);
    addAndMakeVisible (button);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, button);
}

Panel::Panel (const juce::String& t) : title (t)
{
    setInterceptsMouseClicks (false, true);
}

juce::Rectangle<int> Panel::content() const
{
    return getLocalBounds().reduced (8, 0).withTrimmedTop (26).withTrimmedBottom (6);
}

void Panel::setAwake (bool a)
{
    if (a != awake)
    {
        awake = a;
        repaint (getLocalBounds().removeFromTop (26));
    }
}

void Panel::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), cornerRadius);
    auto head = getLocalBounds().reduced (10, 0).removeFromTop (26);
    g.setColour (colours::text);
    g.setFont (titleFont());
    g.drawFittedText (title, head, juce::Justification::centredLeft, 1);
    if (indicator)
    {
        const auto dot = head.removeFromRight (12).withSizeKeepingCentre (8, 8).toFloat();
        g.setColour (awake ? colours::accent : colours::track);
        g.fillEllipse (dot);
        g.setColour (colours::secondary);
        g.setFont (labelFont());
        g.drawFittedText (awake ? "awake" : "asleep", head.removeFromRight (44), juce::Justification::centredRight, 1);
    }
}
} // namespace sph::ui
