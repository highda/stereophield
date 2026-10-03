#pragma once

#include "dsp/OnePole.h"
#include "dsp/Params.h"
#include "dsp/ProcessSpec.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace sph
{
// Output stage of DESIGN.md section 6.5: matrix, loudness compensation,
// output gain, listen mode, and the latency-aligned bypass of section 6.1.
class OutputStage
{
public:
    void prepare (const ProcessSpec& spec);
    void reset();
    void setParams (const Params& p, bool snap);

    void process (const float* mOut, const float* sSyn, const float* mD, const float* sInD,
                  float* outL, float* outR, int numSamples) noexcept;

    // Test hook: last loudness-compensation factor.
    float compensation() const noexcept { return lastC; }

private:
    double fs = 48000.0;
    juce::SmoothedValue<float> gain, compMix, bypassMix;
    OnePole pm2, ps2;
    Listen listen = Listen::Stereo;
    float lastC = 1.0f;
};
} // namespace sph
