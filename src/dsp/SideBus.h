#pragma once

#include "dsp/Biquad.h"
#include "dsp/OnePole.h"
#include "dsp/ProcessSpec.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>

namespace sph
{
struct Params;

// Side bus of DESIGN.md section 6.4, and the mid of section 6.5:
//   S_bus: bass-mono high-pass -> three width bands -> transient duck
//          -> width -> correlation guard -> S_syn
//   D_bus: the same high-pass (and the band split at unity, when it is in
//          use) -> D_syn;  M_out = M_d + mid_blend * D_syn.
class SideBus
{
public:
    void prepare (const ProcessSpec& spec);
    void reset();
    void setParams (const Params& p, bool snap);

    // e may be null (no ducking). Writes S_syn and M_out.
    void process (const float* sBus, const float* dBus, const float* e, const float* mD,
                  float* sSyn, float* mOut, int numSamples) noexcept;

    // While asleep: S_syn = 0 and M_out = M_d; smoothers keep moving.
    void advanceWhileAsleep (int numSamples) noexcept;

    int tailSamples() const noexcept { return (int) std::ceil (0.100 * fs); }

    // True when the side output cannot change any more by smoothing alone.
    bool widthIsZeroAndSettled() const noexcept { return ! width.isSmoothing() && width.getTargetValue() == 0.0f; }
    bool duckIsZeroAndSettled() const noexcept { return ! duck.isSmoothing() && duck.getTargetValue() == 0.0f; }

    // Test hooks.
    float guardGain() const noexcept { return (float) guardG.current(); }
    bool bandSplitActive() const noexcept { return splitMix.getCurrentValue() > 0.0f; }

private:
    void updateFilters() noexcept;

    double fs = 48000.0;

    // Step 1: LR4 high-pass as two Butterworth sections, on S (0) and D (1).
    juce::SmoothedValue<float> hpHz, hpMix;
    std::array<std::array<Biquad, 2>, 2> hp;

    // Step 2: band split.
    juce::SmoothedValue<float> xLo, xHi, bandLow, bandMid, bandHigh, splitMix;
    juce::dsp::LinkwitzRileyFilter<float> lr1, lr2, ap2;
    bool splitTarget = false;

    // Step 3 and 4.
    juce::SmoothedValue<float> duck, width, midBlend, guardMix;
    OnePole guardPm, guardPs;
    Follower guardG;
};
} // namespace sph
