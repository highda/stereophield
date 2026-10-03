#pragma once

#include "dsp/Buses.h"
#include "dsp/DelayLine.h"
#include "dsp/ProcessSpec.h"

#include <array>

namespace sph
{
// Early-reflection width (PART2_LEDGER.md G8): image sources of a shoebox
// room up to second order, at most 80 ms after the direct sound, no tail.
// Each reflection i with gain g and azimuth theta contributes
//   s += g sin(theta) lp(delay(u))     m += g |cos(theta)| lp(delay(u)).
// Geometry changes crossfade between two tap sets on one delay line, so they
// never fade the output to silence.
class RoomCues
{
public:
    static constexpr int maxTaps = 24;
    static constexpr double maxDelaySeconds = 0.080;

    struct Tap
    {
        int delay = 0;
        float side = 0.0f, mid = 0.0f, lpCoeff = 0.0f;
        int hits = 0;
        float azimuthDeg = 0.0f;
    };

    struct TapSet
    {
        std::array<Tap, maxTaps> taps {};
        int count = 0;
    };

    struct Geometry
    {
        float size = 8.0f, distance = 2.0f, absorb = 0.4f, damp = 9000.0f;
        int order = 2;
        bool operator== (const Geometry&) const = default;
    };

    // Image-source model; allocation-free.
    static void build (TapSet& set, const Geometry& g, double sampleRate) noexcept;

    void prepare (const ProcessSpec& spec);
    void reset();
    void setParams (const Params& p, bool snap);
    void process (const Buses& buses, float* s, float* m, int numSamples) noexcept;
    int tailSamples() const noexcept { return (int) std::ceil ((maxDelaySeconds + 0.005) * fs); }

    Source activeSource() const noexcept { return source; }
    bool isSettled() const noexcept { return fadePos >= fadeLen; }

    // Test hook.
    const TapSet& taps() const noexcept { return sets[(size_t) activeSet]; }

private:
    double fs = 48000.0;
    Source source = Source::Full;
    Geometry current, wanted;
    DelayLine line;
    std::array<TapSet, 2> sets;
    std::array<std::array<float, maxTaps>, 2> lpState {};
    int activeSet = 0, fadeLen = 1440, fadePos = 1440;
};
} // namespace sph
