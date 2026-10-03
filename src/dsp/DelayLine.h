#pragma once

#include <vector>

namespace sph
{
// Circular delay line. push() one sample, then read at a delay in samples:
// a delay of 0 returns the sample just pushed.
class DelayLine
{
public:
    // Allocates for delays up to maxDelaySamples (plus interpolation headroom).
    void prepare (int maxDelaySamples);
    void reset();

    void push (float x) noexcept
    {
        buffer[(size_t) writeIndex] = x;
        writeIndex = (writeIndex + 1) & mask;
    }

    float readInt (int delay) const noexcept
    {
        return buffer[(size_t) ((writeIndex - 1 - delay) & mask)];
    }

    float readLinear (double delay) const noexcept;
    float readLagrange (double delay) const noexcept;

    int maxDelay() const noexcept { return maxDelaySamples; }

private:
    std::vector<float> buffer;
    int mask = 0;
    int writeIndex = 0;
    int maxDelaySamples = 0;
};
} // namespace sph
