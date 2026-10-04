#pragma once

#include "ui/Style.h"

#include <array>
#include <vector>

namespace sph
{
class Meters;
class PanMap;
} // namespace sph

namespace sph::ui
{
// Goniometer: (L, R) pairs plotted at
// x = (R - L) / sqrt 2, y = (L + R) / sqrt 2 as dots that fade over 150 ms.
class Goniometer : public juce::Component
{
public:
    void push (const float* pairs, int count, double nowMs);
    void prune (double nowMs);
    void paint (juce::Graphics&) override;

private:
    struct Dot
    {
        float x, y;
        double t;
    };
    std::vector<Dot> dots;
    size_t head = 0, size = 0;
    double now = 0.0;
    static constexpr double fadeMs = 150.0;
    static constexpr size_t capacity = 16384;
};

// Correlation from -1 to +1 with a centre mark; the negative half is drawn in
// the warning colour.
class CorrelationMeter : public juce::Component
{
public:
    void setValue (float v);
    void paint (juce::Graphics&) override;

private:
    float value = 0.0f;
};

// Peak level of L and R, -60 to +6 dB, with a 1.5 s peak hold.
class LevelMeters : public juce::Component
{
public:
    void setPeaks (float l, float r, double nowMs);
    void paint (juce::Graphics&) override;

private:
    struct Channel
    {
        float level = 0.0f, hold = 0.0f;
        double holdTime = 0.0;
    };
    std::array<Channel, 2> ch;
};

// Pan against log frequency, brightness by magnitude.
class PanMapDisplay : public juce::Component
{
public:
    void update (const PanMap& pm, bool fullEngine);
    void paint (juce::Graphics&) override;

private:
    std::array<float, 128> pan {}, mag {};
    bool full = false;
};
} // namespace sph::ui
