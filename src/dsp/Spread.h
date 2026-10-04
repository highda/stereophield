#pragma once

#include "dsp/Biquad.h"
#include "dsp/Buses.h"
#include "dsp/ChangeFader.h"
#include "dsp/DelayLine.h"
#include "dsp/OnePole.h"
#include "dsp/ProcessSpec.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

namespace sph
{
// All-pass side engine: s = A(u), m = 0.
class Spread
{
public:
    static constexpr int maxSections = 24;

    struct Structure
    {
        Source source = Source::Full;
        SpreadType type = SpreadType::Cascade;
        int density = 8;
        bool operator== (const Structure&) const = default;
    };

    void prepare (const ProcessSpec& spec);
    void reset();
    void setParams (const Params& p, bool snap);
    void process (const Buses& buses, float* s, int numSamples) noexcept;
    int tailSamples() const noexcept;

    const Structure& active() const noexcept { return current; }
    const Structure& pending() const noexcept { return wanted; }
    bool isSettled() const noexcept { return fader.isSettled(); }

    // When set, structural changes are left to the owner (which crossfades
    // between two instances) instead of the internal fade.
    bool externalSwitching = false;

    // Test hook: centre frequency of cascade section i.
    double sectionFrequency (int i) const noexcept { return freqs[(size_t) i]; }

private:
    void updateCoefficients() noexcept;
    void resetState() noexcept;

    double fs = 48000.0;
    Structure current, wanted;
    ChangeFader fader;
    DelayLine delay;
    OnePole timeSmoother;
    float timeTargetSamples = 0.0f;
    juce::SmoothedValue<float> fLo, fHi, skew, q;
    std::array<Biquad, maxSections> sections;
    std::array<double, maxSections> freqs {};
    std::array<double, maxSections> radii {};
    bool coeffsDirty = true;
};
} // namespace sph
