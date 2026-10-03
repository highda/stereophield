#pragma once

#include "dsp/Params.h"
#include "dsp/PartialTracker.h"
#include "dsp/SourceGrouper.h"

#include <array>
#include <atomic>
#include <complex>
#include <vector>

namespace sph
{
// Adaptive spectral panning (DESIGN.md section 6.3.5). Works on frames of the
// Full analysis and writes the side spectrum S(t, k).
class PanMap
{
public:
    static constexpr int displayPoints = 128;

    void prepare (double sampleRate, int fftSize, int hop);
    void reset();

    void setOwnership (PanOwnership o) noexcept { ownership = o; }

    void setParams (PanMode mode, double depth, double density, double bassCentreHz, int maxGroups) noexcept
    {
        panMode = mode;
        panDepth = depth;
        panDensity = density;
        bassCentre = bassCentreHz;
        groupLimit = maxGroups;
    }

    // Per-bin zero-phase weights applied to the side spectrum (the side bus's
    // bass mono and band widths); see SideBus::spectralWeights.
    float* weights() noexcept { return weight.data(); }

    // One frame: X spectrum, A magnitudes, tonal mask mt2, noise mask mn2.
    // Writes the side spectrum into s.
    void processFrame (const std::complex<float>* x, const float* a, const float* mt2, const float* mn2,
                       std::complex<float>* s) noexcept;

    // Display data for the user interface: pan and magnitude at 128
    // log-spaced frequencies from 40 Hz to 16 kHz.
    float displayPan (int i) const noexcept { return dispPan[(size_t) i].load (std::memory_order_relaxed); }
    float displayMagnitude (int i) const noexcept { return dispMag[(size_t) i].load (std::memory_order_relaxed); }
    static double displayFrequency (int i) noexcept;

    // Test hooks.
    const PartialTracker& tracker() const noexcept { return partials; }
    const SourceGrouper& grouper() const noexcept { return sources; }
    const std::vector<float>& pan() const noexcept { return pSmooth; }
    int frame() const noexcept { return frameIndex; }

private:
    double curve (double f) const noexcept;

    double fs = 48000.0;
    int n = 2048, k = 1025;
    PanMode panMode = PanMode::Groups;
    PanOwnership ownership = PanOwnership::Soft;
    double panDepth = 0.7, panDensity = 1.0, bassCentre = 120.0;
    int groupLimit = 6;
    double timeCoeff = 0.0;
    int ownRadius = 6;
    int frameIndex = 0;

    PartialTracker partials;
    SourceGrouper sources;
    std::vector<float> at, target, p, pSmooth, weight, wsum;
    std::array<int, PartialTracker::maxTracks> owners {};
    std::array<int, displayPoints> dispBin {};
    std::array<std::atomic<float>, displayPoints> dispPan {}, dispMag {};
};
} // namespace sph
