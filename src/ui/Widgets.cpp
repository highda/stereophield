#include "ui/Widgets.h"

namespace sph::ui
{
void setHelpKey (juce::Component& c, const juce::String& key) { c.getProperties().set ("help", key); }
juce::String helpKeyOf (const juce::Component& c) { return c.getProperties()["help"].toString(); }

juce::String shortHelp (const juce::String& key)
{
    Help h;
    if (! help (key, h))
        return {};
    auto s = h.what;
    const int dot = s.indexOf (". ");
    return dot > 0 ? s.substring (0, dot + 1) : s;
}

void localiseTree (juce::Component& root)
{
    if (auto* l = dynamic_cast<Localisable*> (&root))
        l->localise();
    for (auto* c : root.getChildren())
        localiseTree (*c);
}

// ------------------------------------------------------------------ Knob
Knob::Knob (APVTS& state, const juce::String& paramId, bool large) : id (paramId), big (large)
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 14);
    addAndMakeVisible (knob);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, knob);
    baseText = knob.textFromValueFunction;
    auto baseValue = knob.valueFromTextFunction;
    knob.textFromValueFunction = [this] (double v) { return localNumber (baseText ? baseText (v) : juce::String (v)); };
    knob.valueFromTextFunction = [baseValue] (const juce::String& t)
    {
        const auto txt = t.replaceCharacter (',', '.');
        return baseValue ? baseValue (txt) : txt.getDoubleValue();
    };
    if (auto* p = state.getParameter (paramId))
        knob.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    setHelpKey (*this, "p." + paramId);
    setHelpKey (knob, "p." + paramId);
    localise();
}

void Knob::localise()
{
    const auto cap = tr ("cap." + id);
    knob.setTitle (cap);
    knob.setDescription (shortHelp ("p." + id));
    knob.setTooltip (shortHelp ("p." + id));
    knob.updateText();
    repaint();
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
    g.drawFittedText (tr ("cap." + id), getLocalBounds().removeFromTop (14), juce::Justification::centred, 1, 0.6f);
}

// ---------------------------------------------------------------- Choice
Choice::Choice (APVTS& state, const juce::String& paramId, bool showCaption) : id (paramId), caption (showCaption)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        items = p->choices.size();
    for (int i = 0; i < items; ++i)
        combo.addItem (tr (itemKey (i)), i + 1);
    addAndMakeVisible (combo);
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramId, combo);
    setHelpKey (*this, "p." + paramId);
    setHelpKey (combo, "p." + paramId);
    localise();
}

juce::String Choice::itemKey (int index) const
{
    const bool source = id.endsWith ("_source");
    return "choice." + (source ? juce::String ("source") : id) + "." + juce::String (index);
}

void Choice::localise()
{
    // The shown text is cached until the selection is set again, and
    // getSelectedId() reads 0 once the text no longer matches, so the
    // selection is taken before renaming.
    const int selectedId = combo.getSelectedId();
    for (int i = 0; i < items; ++i)
        combo.changeItemText (i + 1, tr (itemKey (i)));
    if (selectedId != 0)
        combo.setSelectedId (selectedId, juce::dontSendNotification);
    combo.setTitle (tr ("cap." + id));
    combo.setDescription (shortHelp ("p." + id));
    combo.setTooltip (shortHelp ("p." + id));
    repaint();
}

void Choice::resized()
{
    auto r = getLocalBounds();
    if (caption)
        r.removeFromLeft (juce::jmin (52, r.getWidth() / 3));
    combo.setBounds (r);
}

void Choice::paint (juce::Graphics& g)
{
    if (! caption)
        return;
    g.setColour (colours::secondary);
    g.setFont (labelFont());
    g.drawFittedText (tr ("cap." + id), getLocalBounds().removeFromLeft (juce::jmin (52, getWidth() / 3)),
                      juce::Justification::centredLeft, 1, 0.8f);
}

// ------------------------------------------------------------- Segmented
Segmented::Segmented (APVTS* state, const juce::String& paramId, std::vector<juce::String> labelKeys) : keys (std::move (labelKeys))
{
    for (size_t i = 0; i < keys.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton());
        b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i + 1 < keys.size() ? juce::Button::ConnectedOnRight : 0));
        b->onClick = [this, i]
        {
            if (attachment != nullptr)
                attachment->setValueAsCompleteGesture ((float) i);
            else
            {
                select ((int) i);
                if (onSelect)
                    onSelect ((int) i);
            }
        };
        addAndMakeVisible (b);
    }
    if (state != nullptr && paramId.isNotEmpty())
        if (auto* param = state->getParameter (paramId))
        {
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
            {
                select ((int) std::lround (v));
                if (onSelect)
                    onSelect (current);
            });
            attachment->sendInitialUpdate();
            setHelpKey (*this, "p." + paramId);
        }
    localise();
}

Segmented::~Segmented() = default;

void Segmented::select (int index)
{
    current = index;
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == index, juce::dontSendNotification);
}

void Segmented::localise()
{
    for (int i = 0; i < buttons.size(); ++i)
    {
        buttons[i]->setButtonText (tr (keys[(size_t) i]));
        buttons[i]->setTitle (tr (keys[(size_t) i]));
        if (helpKeyOf (*this).isNotEmpty())
            setHelpKey (*buttons[i], helpKeyOf (*this));
    }
}

void Segmented::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / juce::jmax (1, buttons.size());
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds (i + 1 == buttons.size() ? r : r.removeFromLeft (w));
}

// ---------------------------------------------------------------- Toggle
Toggle::Toggle (APVTS& state, const juce::String& paramId, const juce::String& labelKey) : key (labelKey)
{
    button.setClickingTogglesState (true);
    addAndMakeVisible (button);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, button);
    setHelpKey (*this, "p." + paramId);
    setHelpKey (button, "p." + paramId);
    localise();
}

void Toggle::localise()
{
    button.setButtonText (tr (key));
    button.setTitle (tr (key));
    button.setTooltip (shortHelp (helpKeyOf (*this)));
}

// ----------------------------------------------------------------- Panel
Panel::Panel (const juce::String& titleKey) : key (titleKey)
{
    setInterceptsMouseClicks (true, true);
    setHelpKey (*this, titleKey.startsWith ("card.") ? titleKey : juce::String());
}

juce::Rectangle<int> Panel::content() const
{
    return getLocalBounds().reduced (6, 0).withTrimmedTop (24).withTrimmedBottom (6);
}

void Panel::setAwake (bool a)
{
    if (a != awake)
    {
        awake = a;
        repaint (getLocalBounds().removeFromTop (24));
    }
}

void Panel::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), cornerRadius);
    if (selected)
    {
        g.setColour (colours::accent.withAlpha (0.8f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.75f), cornerRadius, 1.5f);
    }
    auto head = getLocalBounds().reduced (8, 0).removeFromTop (24);
    if (indicator)
    {
        const auto dot = head.removeFromRight (10).withSizeKeepingCentre (7, 7).toFloat();
        g.setColour (awake ? colours::accent : colours::track);
        g.fillEllipse (dot);
    }
    g.setColour (colours::text);
    g.setFont (titleFont());
    g.drawFittedText (tr (key), head, juce::Justification::centredLeft, 1, 0.55f);
}
} // namespace sph::ui
