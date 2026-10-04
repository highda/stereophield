#pragma once

#include "dsp/Buses.h"
#include "dsp/DelayLine.h"
#include "dsp/ProcessSpec.h"

#include <array>

namespace sph
{
// Early-reflection width: image sources of a shoebox
// room up to second order, at most 80 ms after the direct sound, no tail,
// received by a virtual ORTF pair (two cardioids 17 cm apart, axes at +-55
// degrees) placed off the room's centre lines. Each reflection reaches the two microphones with its own delay
// and directivity gain, which is what decorrelates a symmetric room's
// reflections. With the two microphone signals a and b:
//   s = (a - b) / 2      m = (a + b) / 2
// Geometry changes crossfade between two tap sets on one delay line.
class RoomCues
{
public:
    static constexpr int maxReflections = 24;
    static constexpr double maxDelaySeconds = 0.080;
    static constexpr double micSpacing = 0.17, micAngleDeg = 55.0;
    static constexpr double asymX = 0.40, asymY = 0.45; // array position as fractions of width and depth

    struct Tap
    {
        int delay[2] = { 0, 0 };     // left and right microphone, samples
        float gain[2] = { 0, 0 };
        float lpCoeff = 0.0f;
        int hits = 0;
        float azimuthDeg = 0.0f;     // arrival direction at the array centre
    };

    struct TapSet
    {
        std::array<Tap, maxReflections> taps {};
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
    std::array<std::array<std::array<float, 2>, maxReflections>, 2> lpState {};
    int activeSet = 0, fadeLen = 1440, fadePos = 1440;
};
} // namespace sph
