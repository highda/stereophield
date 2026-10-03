#pragma once

#include "dsp/Buses.h"
#include "dsp/ChangeFader.h"
#include "dsp/DelayLine.h"
#include "dsp/OnePole.h"
#include "dsp/ProcessSpec.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace sph
{
// Haas delay (DESIGN.md section 6.3.2):
//   d = lowpass(delay(u)), s = q (u - d) / 2, m = (d - u) / 2.
class HaasDelay
{
public:
    struct Structure
    {
        Source source = Source::Full;
        HaasSide side = HaasSide::Right;
        bool operator== (const Structure&) const = default;
    };

    void prepare (const ProcessSpec& spec);
    void reset();
    void setParams (const Params& p, bool snap);
    void process (const Buses& buses, float* s, float* m, int numSamples) noexcept;
    int tailSamples() const noexcept;

    const Structure& active() const noexcept { return current; }
    const Structure& pending() const noexcept { return wanted; }
    bool isSettled() const noexcept { return fader.isSettled(); }

    // When set, structural changes are left to the owner (which crossfades
    // between two instances) instead of the internal fade.
    bool externalSwitching = false;

    // Test hook: current smoothed delay in samples.
    double currentDelaySamples() const noexcept { return timeSmoother.current(); }

private:
    double fs = 48000.0;
    Structure current, wanted;
    ChangeFader fader;
    DelayLine delay;
    OnePole timeSmoother;
    float timeTargetSamples = 720.0f;
    juce::SmoothedValue<float> lpHz;
    double lpState = 0.0;
};
} // namespace sph
