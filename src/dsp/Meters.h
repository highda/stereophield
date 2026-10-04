#pragma once

#include "dsp/OnePole.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <vector>

namespace sph
{
// Output meters The audio thread writes atomics and a
// single-producer single-consumer FIFO; the user interface reads them.
class Meters
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (const float* l, const float* r, int numSamples) noexcept;

    // Message thread: peak since the last call, per channel.
    float takePeak (int channel) noexcept { return peaks[channel].exchange (0.0f, std::memory_order_relaxed); }
    float correlation() const noexcept { return corr.load (std::memory_order_relaxed); }

    // Message thread: copies up to maxPairs (L, R) pairs into dst (interleaved).
    int popGoniometer (float* dst, int maxPairs) noexcept;

    static constexpr int fifoPairs = 8192;

private:
    OnePole lr, ll, rr;
    std::atomic<float> peaks[2] {};
    std::atomic<float> corr { 0.0f };
    juce::AbstractFifo fifo { fifoPairs };
    std::vector<float> fifoData;
    int decimation = 1, decimCount = 0;
};
} // namespace sph
