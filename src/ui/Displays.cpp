#include "ui/Displays.h"

#include "dsp/PanMap.h"

#include <cmath>

namespace sph::ui
{
namespace
{
void drawFrame (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (colours::background);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::track);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

float toDb (float a) { return a > 1.0e-6f ? 20.0f * std::log10 (a) : -120.0f; }
} // namespace

void Goniometer::push (const float* pairs, int count, double nowMs)
{
    if (dots.empty())
        dots.resize (capacity);
    constexpr float k = 0.70710678f;
    for (int i = 0; i < count; ++i)
    {
        const float l = pairs[2 * i], r = pairs[2 * i + 1];
        dots[head] = { (r - l) * k, (l + r) * k, nowMs };
        head = (head + 1) % capacity;
        size = std::min (size + 1, capacity);
    }
    now = nowMs;
}

void Goniometer::prune (double nowMs)
{
    now = nowMs;
    while (size > 0)
    {
        const size_t oldest = (head + capacity - size) % capacity;
        if (now - dots[oldest].t <= fadeMs)
            break;
        --size;
    }
}

void Goniometer::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    drawFrame (g, r);
    const auto c = r.getCentre();
    const float radius = r.getWidth() * 0.5f - 6.0f;

    // Axes: M vertical, S horizontal, L and R on the diagonals.
    g.setColour (colours::track);
    g.drawLine (c.x, r.getY() + 6, c.x, r.getBottom() - 6, 1.0f);
    g.drawLine (r.getX() + 6, c.y, r.getRight() - 6, c.y, 1.0f);
    const float d = radius * 0.70710678f;
    g.drawLine (c.x - d, c.y - d, c.x + d, c.y + d, 1.0f);
    g.drawLine (c.x - d, c.y + d, c.x + d, c.y - d, 1.0f);
    g.drawEllipse (c.x - radius, c.y - radius, radius * 2, radius * 2, 1.0f);
    g.setColour (colours::secondary);
    g.setFont (labelFont());
    g.drawText ("L", juce::Rectangle<float> (c.x - d - 14, c.y - d - 14, 12, 12), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (c.x + d + 2, c.y - d - 14, 12, 12), juce::Justification::centred);
    g.drawText ("M", juce::Rectangle<float> (c.x + 3, r.getY() + 4, 12, 12), juce::Justification::centredLeft);
    g.drawText ("S", juce::Rectangle<float> (r.getRight() - 16, c.y + 2, 12, 12), juce::Justification::centred);

    // Full scale (|L| = |R| = 1) reaches the circle.
    const float scale = radius / 1.41421356f;
    for (size_t i = 0; i < size; ++i)
    {
        const auto& p = dots[(head + capacity - size + i) % capacity];
        const float age = (float) ((now - p.t) / fadeMs);
        if (age > 1.0f)
            continue;
        const float x = c.x + juce::jlimit (-radius, radius, p.x * scale);
        const float y = c.y - juce::jlimit (-radius, radius, p.y * scale);
        g.setColour (colours::accent.withAlpha (0.75f * (1.0f - age)));
        g.fillRect (x - 0.75f, y - 0.75f, 1.5f, 1.5f);
    }
}

void CorrelationMeter::setValue (float v)
{
    v = juce::jlimit (-1.0f, 1.0f, v);
    if (std::abs (v - value) > 0.002f)
    {
        value = v;
        repaint();
    }
}

void CorrelationMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto label = r.removeFromLeft (26);
    g.setColour (colours::secondary);
    g.setFont (labelFont());
    g.drawText ("-1", label, juce::Justification::centredLeft);
    g.drawText ("+1", r.removeFromRight (20), juce::Justification::centredRight);
    r.reduce (2, 2);
    drawFrame (g, r);
    const float mid = r.getCentreX();
    const float x = mid + value * r.getWidth() * 0.5f;
    g.setColour (value < 0.0f ? colours::warning : colours::accent);
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (std::min (mid, x), r.getY() + 3, std::max (mid, x), r.getBottom() - 3));
    g.setColour (colours::text);
    g.fillRect (mid - 0.5f, r.getY() + 1, 1.0f, r.getHeight() - 2);
}

void LevelMeters::setPeaks (float l, float r, double nowMs)
{
    const float in[2] = { l, r };
    for (int i = 0; i < 2; ++i)
    {
        auto& c = ch[(size_t) i];
        // Fall at about 24 dB per second between peaks.
        c.level = std::max (in[i], c.level * 0.83f);
        if (in[i] >= c.hold || nowMs - c.holdTime > 1500.0)
        {
            c.hold = in[i];
            c.holdTime = nowMs;
        }
    }
    repaint();
}

void LevelMeters::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float rowH = (r.getHeight() - 4.0f) * 0.5f;
    const char* names[] = { "L", "R" };
    for (int i = 0; i < 2; ++i)
    {
        auto row = r.removeFromTop (rowH);
        if (i == 0)
            r.removeFromTop (4.0f);
        g.setColour (colours::secondary);
        g.setFont (labelFont());
        g.drawText (names[i], row.removeFromLeft (14), juce::Justification::centredLeft);
        auto scale = row.removeFromRight (34);
        if (i == 0)
            g.drawText ("+6 dB", scale, juce::Justification::centredRight);
        drawFrame (g, row);
        auto bar = row.reduced (2, 2);
        auto pos = [&] (float a) { return juce::jlimit (0.0f, 1.0f, (toDb (a) + 60.0f) / 66.0f) * bar.getWidth(); };
        const auto& c = ch[(size_t) i];
        const float w = pos (c.level);
        const float zero = bar.getX() + 60.0f / 66.0f * bar.getWidth();
        g.setColour (colours::accent);
        g.fillRect (bar.withWidth (w));
        if (bar.getX() + w > zero)
        {
            g.setColour (colours::warning);
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (zero, bar.getY(), bar.getX() + w, bar.getBottom()));
        }
        g.setColour (colours::text);
        const float hx = bar.getX() + pos (c.hold);
        if (c.hold > 1.0e-6f)
            g.fillRect (hx - 1.0f, bar.getY(), 2.0f, bar.getHeight());
        g.setColour (colours::track);
        g.fillRect (zero, bar.getY(), 1.0f, bar.getHeight());
    }
}

void PanMapDisplay::update (const PanMap& pm, bool fullEngine)
{
    full = fullEngine;
    for (int i = 0; i < 128; ++i)
    {
        pan[(size_t) i] = pm.displayPan (i);
        mag[(size_t) i] = pm.displayMagnitude (i);
    }
    repaint();
}

void PanMapDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawFrame (g, r);
    auto plot = r.reduced (34, 10).withTrimmedBottom (8);

    g.setFont (labelFont());
    g.setColour (colours::secondary);
    g.drawText ("L", juce::Rectangle<float> (r.getX() + 8, plot.getY() - 6, 20, 12), juce::Justification::centredLeft);
    g.drawText ("R", juce::Rectangle<float> (r.getX() + 8, plot.getBottom() - 6, 20, 12), juce::Justification::centredLeft);
    g.setColour (colours::track);
    g.drawLine (plot.getX(), plot.getCentreY(), plot.getRight(), plot.getCentreY(), 1.0f);

    // Frequency grid.
    auto xOf = [&] (double f) { return plot.getX() + (float) (std::log (f / 40.0) / std::log (16000.0 / 40.0)) * plot.getWidth(); };
    for (double f : { 100.0, 1000.0, 10000.0 })
    {
        g.setColour (colours::track);
        g.drawLine (xOf (f), plot.getY(), xOf (f), plot.getBottom(), 1.0f);
        g.setColour (colours::secondary);
        g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + " kHz" : juce::String ((int) f) + " Hz",
                    juce::Rectangle<float> (xOf (f) + 3, plot.getBottom() - 1, 50, 12), juce::Justification::centredLeft);
    }

    if (! full)
    {
        g.setColour (colours::secondary);
        g.setFont (bodyFont());
        g.drawText ("Full engine only", plot, juce::Justification::centred);
        return;
    }

    float maxMag = 1.0e-6f;
    for (float m : mag)
        maxMag = std::max (maxMag, m);
    for (int i = 0; i < 128; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / 127.0f;
        const float y = plot.getCentreY() - pan[(size_t) i] * plot.getHeight() * 0.5f;
        // Brightness on a 60 dB scale below the loudest point.
        const float b = juce::jlimit (0.0f, 1.0f, 1.0f + toDb (mag[(size_t) i] / maxMag) / 60.0f);
        if (b <= 0.0f)
            continue;
        g.setColour (colours::accent.withAlpha (0.15f + 0.85f * b));
        g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
    }
}
} // namespace sph::ui
