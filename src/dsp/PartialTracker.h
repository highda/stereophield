#pragma once

#include <array>

namespace sph
{
// Stage C of DESIGN.md section 6.2.5: peak picking on the tonal magnitudes and
// frame-to-frame partial tracking in 96 fixed slots.
class PartialTracker
{
public:
    static constexpr int maxTracks = 96;
    static constexpr int maxPeaks = 60;
    static constexpr double matchTolerance = 0.03; // tunable

    struct Track
    {
        bool active = false;
        double freq = 0.0, birthFreq = 0.0, amp = 0.0;
        int birthFrame = 0, age = 0, missed = 0;
        int group = -1;
        double pan = 0.0;
        int peakBin = 0; // bin of the peak matched this frame, or nearest bin
    };

    void prepare (double sampleRate, int fftSize);
    void reset();

    // at: tonal magnitudes mt2 * A for numBins bins; frame: frame index.
    void process (const float* at, int numBins, int frame) noexcept;

    std::array<Track, maxTracks>& tracks() noexcept { return slots; }
    const std::array<Track, maxTracks>& tracks() const noexcept { return slots; }
    int numActive() const noexcept;

private:
    struct Peak
    {
        double freq, amp;
        int bin;
        bool claimed;
    };

    double fs = 48000.0;
    int n = 2048;
    std::array<Track, maxTracks> slots;
    std::array<Peak, maxPeaks> peaks {};
    int numPeaks = 0;
    std::array<int, maxTracks> order {};
};
} // namespace sph
