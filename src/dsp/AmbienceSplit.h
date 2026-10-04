#pragma once

#include <vector>

namespace sph
{
// Stage B: energy that is decaying is moved from
// the tonal mask to the noise mask.
class AmbienceSplit
{
public:
    // Tunable smoothing constants: Ps = keep * Ps + (1 - keep) * Pw.
    static constexpr double keep = 0.7;

    void prepare (int numBins, double sampleRate, int hop);
    void reset();

    // a: magnitudes; mt and mn are updated in place to mt2 and mn2; ma
    // receives the ambience mask.
    void process (const float* a, float* mt, float* mn, float* ma, double ambience, double roomDecaySeconds) noexcept;

private:
    int k = 0, delayFrames = 1, pos = 0;
    std::vector<float> ps; // (delayFrames + 1) x k ring of smoothed power
};
} // namespace sph
