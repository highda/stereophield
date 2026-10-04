#pragma once

#include <vector>

namespace sph
{
// Stage A: median-filter split of each frame's
// magnitudes into tonal, transient and noise masks that sum to 1, using only
// the current and past frames.
class ComponentSplit
{
public:
    // Tunable constants.
    static constexpr int freqMedian = 17;  // bins, odd
    static constexpr int timeMedian = 9;   // frames, current included
    static constexpr double tonalBreak = 0.45;
    static constexpr double transientBreak = 0.55;
    static constexpr double breakWidth = 0.30;

    void prepare (int numBins);

    // The state after reset equals the state after a long silence, where
    // every frame is zero and so every bin is transient.
    void reset();

    // a: magnitudes of the current frame. Writes the three masks.
    void process (const float* a, float* mt, float* mx, float* mn) noexcept;

private:
    int k = 0;
    int histPos = 0;
    std::vector<float> history; // timeMedian x k
    std::vector<float> mtPrev, mxPrev;
};
} // namespace sph
