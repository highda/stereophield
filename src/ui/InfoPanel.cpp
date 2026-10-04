#include "ui/InfoPanel.h"

namespace sph::ui
{
InfoPanel::InfoPanel()
{
    for (auto* b : { &pin, &back, &next, &apply, &close })
        addAndMakeVisible (*b);
    pin.setClickingTogglesState (true);
    back.onClick = [this] { if (tour >= 0 && step > 0) { --step; scroll = 0.0f; if (onTourChanged) onTourChanged(); repaint(); resized(); } };
    next.onClick = [this]
    {
        if (tour >= 0 && step + 1 < (int) tours()[(size_t) tour].steps.size())
        {
            ++step;
            scroll = 0.0f;
        }
        if (onTourChanged)
            onTourChanged();
        repaint();
        resized();
    };
    apply.onClick = [this] { if (auto* s = currentStep(); s != nullptr && onApply) onApply (*s); };
    close.onClick = [this]
    {
        tour = -1;
        listing = false;
        if (onTourChanged)
            onTourChanged();
        resized();
        repaint();
    };
    for (size_t i = 0; i < tours().size(); ++i)
    {
        auto* b = tourButtons.add (new juce::TextButton());
        b->onClick = [this, i] { startTour ((int) i); };
        addChildComponent (b);
    }
    setHelpKey (*this, "ui.learn");
    localise();
}

void InfoPanel::localise()
{
    pin.setButtonText (tr ("ui.pin"));
    back.setButtonText (tr ("ui.back"));
    next.setButtonText (tr ("ui.next"));
    apply.setButtonText (tr ("ui.apply"));
    close.setButtonText (tr ("ui.close"));
    for (int i = 0; i < tourButtons.size(); ++i)
    {
        const auto& t = tours()[(size_t) i];
        tourButtons[i]->setButtonText (juce::String::fromUTF8 (language() == Language::Czech ? t.titleCs : t.titleEn));
    }
    repaint();
}

void InfoPanel::show (const juce::String& helpKey, const juce::String& liveText)
{
    if (isPinned() || tour >= 0 || listing)
        return;
    if (helpKey == key && liveText == live)
        return;
    if (helpKey != key)
        scroll = 0.0f;
    key = helpKey;
    live = liveText;
    repaint();
}

void InfoPanel::openTourList()
{
    listing = true;
    tour = -1;
    resized();
    repaint();
}

void InfoPanel::startTour (int index)
{
    listing = false;
    tour = index;
    step = 0;
    scroll = 0.0f;
    if (onTourChanged)
        onTourChanged();
    resized();
    repaint();
}

const TourStep* InfoPanel::currentStep() const
{
    return tour >= 0 ? &tours()[(size_t) tour].steps[(size_t) step] : nullptr;
}

void InfoPanel::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto bottom = r.removeFromBottom (24);
    const bool inT = tour >= 0;
    pin.setVisible (! inT && ! listing);
    for (auto* b : { &back, &next, &apply })
        b->setVisible (inT);
    close.setVisible (inT || listing);
    if (inT)
    {
        const int w = bottom.getWidth() / 4;
        back.setBounds (bottom.removeFromLeft (w).reduced (2, 0));
        next.setBounds (bottom.removeFromLeft (w).reduced (2, 0));
        apply.setBounds (bottom.removeFromLeft (w).reduced (2, 0));
        close.setBounds (bottom.reduced (2, 0));
        apply.setEnabled (currentStep() != nullptr && ! currentStep()->apply.empty());
    }
    else if (listing)
        close.setBounds (bottom.removeFromRight (80));
    else
        pin.setBounds (bottom.removeFromRight (80));
    auto list = r.withTrimmedTop (40);
    for (auto* b : tourButtons)
    {
        b->setVisible (listing);
        b->setBounds (list.removeFromTop (28).reduced (0, 2));
    }
}

juce::AttributedString InfoPanel::compose() const
{
    juce::AttributedString a;
    a.setWordWrap (juce::AttributedString::byWord);
    auto add = [&a] (const juce::String& t, const juce::Font& f, juce::Colour c) { a.append (t, f, c); };
    const auto heading = juce::Font (juce::FontOptions (11.0f, juce::Font::bold));
    if (tour >= 0)
    {
        const auto& t = tours()[(size_t) tour];
        const auto& s = t.steps[(size_t) step];
        add (juce::String::fromUTF8 (language() == Language::Czech ? t.titleCs : t.titleEn) + "\n", juce::Font (juce::FontOptions (15.0f, juce::Font::bold)), colours::text);
        add (tr ("ui.step") + " " + juce::String (step + 1) + " / " + juce::String ((int) t.steps.size()) + "\n\n", heading, colours::secondary);
        add (juce::String::fromUTF8 (language() == Language::Czech ? s.cs : s.en), bodyFont(), colours::text);
        return a;
    }
    if (listing)
    {
        add (tr ("ui.tours"), juce::Font (juce::FontOptions (15.0f, juce::Font::bold)), colours::text);
        return a;
    }
    Help h;
    if (! help (key, h))
        return a;
    add (h.title + "\n\n", juce::Font (juce::FontOptions (15.0f, juce::Font::bold)), colours::text);
    if (h.what.isNotEmpty())
    {
        add (tr ("ui.whatitdoes") + "\n", heading, colours::secondary);
        add (h.what + "\n\n", bodyFont(), colours::text);
    }
    if (live.isNotEmpty())
    {
        add (tr ("ui.now") + "\n", heading, colours::secondary);
        add (live + "\n\n", bodyFont(), colours::accent);
    }
    if (h.how.isNotEmpty())
    {
        add (tr ("ui.how") + "\n", heading, colours::secondary);
        add (h.how + "\n\n", bodyFont(), colours::text);
    }
    if (h.tryThis.isNotEmpty())
    {
        add (tr ("ui.try") + "\n", heading, colours::secondary);
        add (h.tryThis + "\n\n", bodyFont(), colours::text);
    }
    if (h.deep.isNotEmpty())
    {
        add (tr ("ui.deep") + "\n", heading, colours::secondary);
        add (h.deep + "\n\n", bodyFont(), colours::text);
    }
    if (h.ref.isNotEmpty())
    {
        add (tr ("ui.ref") + "\n", heading, colours::secondary);
        add (h.ref, labelFont(), colours::secondary);
    }
    return a;
}

void InfoPanel::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), cornerRadius);
    auto r = textArea().toFloat();
    juce::TextLayout layout;
    layout.createLayout (compose(), r.getWidth() - 6.0f);
    contentHeight = layout.getHeight();
    scroll = juce::jlimit (0.0f, maxScroll(), scroll);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (r.toNearestInt());
        layout.draw (g, r.withTrimmedRight (6.0f).withHeight (contentHeight).translated (0.0f, -scroll));
    }
    if (maxScroll() > 0.0f)
    {
        // A thin scroll indicator on the right.
        const float frac = r.getHeight() / contentHeight;
        const float barH = std::max (20.0f, r.getHeight() * frac);
        const float y = r.getY() + (r.getHeight() - barH) * (scroll / maxScroll());
        g.setColour (colours::track);
        g.fillRoundedRectangle (r.getRight() - 3.0f, r.getY(), 3.0f, r.getHeight(), 1.5f);
        g.setColour (colours::secondary);
        g.fillRoundedRectangle (r.getRight() - 3.0f, y, 3.0f, barH, 1.5f);
    }
}

juce::Rectangle<int> InfoPanel::textArea() const
{
    return getLocalBounds().reduced (12).withTrimmedBottom (30);
}

float InfoPanel::maxScroll() const
{
    return std::max (0.0f, contentHeight - (float) textArea().getHeight());
}

void InfoPanel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    scrollBy (-w.deltaY * 120.0f);
}

void InfoPanel::scrollBy (float points)
{
    const float before = scroll;
    scroll = juce::jlimit (0.0f, maxScroll(), scroll + points);
    if (scroll != before)
        repaint();
}

float InfoPanel::measure()
{
    juce::TextLayout layout;
    layout.createLayout (compose(), (float) textArea().getWidth() - 6.0f);
    contentHeight = layout.getHeight();
    return contentHeight;
}

float InfoPanel::textHeight (const juce::String& helpKey, float width)
{
    InfoPanel p;
    p.key = helpKey;
    p.live = "At 15.0 ms the comb notches are 66.7 Hz apart; the first is at 33.3 Hz.";
    juce::TextLayout layout;
    layout.createLayout (p.compose(), width);
    return layout.getHeight();
}
} // namespace sph::ui
