#pragma once

#include "dsp/DelayLine.h"
#include "dsp/Params.h"
#include "dsp/Velvet.h"

#include <array>
#include <atomic>
#include <complex>
#include <vector>

namespace sph
{
// Target-coherence stereo synthesis (PART2_LEDGER.md G6). With the dry mid M
// and a decorrelated copy D, L = M + a D and R = M - a D keep (L + R) / 2 = M
// exactly while the inter-channel coherence follows a target curve c(f). The
// gain a is solved per ERB band from measured statistics; the part of D in
// phase with M is removed first, so the target is reachable even for tones.
class CoherenceDesigner
{
public:
    static constexpr int numBands = 40;
    static constexpr double alphaMax = 16.0;

    void prepare (double sampleRate, int fftSize, int hop);
    void reset();
    void setParams (const Params& p) noexcept;

    // Time domain, once per sample on the undelayed mid: returns the
    // decorrelated sample to feed the D analysis.
    float decorrelate (float m) noexcept;

    // One frame. x: spectrum of M; d: spectrum of D; masks of stage A and B.
    // Writes the side spectrum S = a(k) * mask(k) * D(k).
    void processFrame (const std::complex<float>* x, const std::complex<float>* d, const float* mt2,
                       const float* mn2, const float* mx, std::complex<float>* s) noexcept;

    // Target coherence at a frequency for the current settings.
    double target (double f) const noexcept;

    // Display and test hooks, per band.
    static double bandCentre (int b, double sampleRate) noexcept; // of the unmerged ERB grid
    // Band layout for a rate and FFT size: ERB-spaced edges from 50 Hz to
    // 20 kHz (or 0.45 fs), merged so that every band spans at least three
    // bins. Returns the number of bands; edges in bins and centres in Hz.
    static int layout (double sampleRate, int fftSize, int* edgeBins, double* centresHz) noexcept;
    float targetCoherence (int b) const noexcept { return dispTarget[(size_t) b].load (std::memory_order_relaxed); }
    float achievedCoherence (int b) const noexcept { return dispAchieved[(size_t) b].load (std::memory_order_relaxed); }
    float bandAlpha (int b) const noexcept { return alpha[(size_t) b]; }
    int numActiveBands() const noexcept { return activeBands; }

private:
    double fs = 48000.0;
    int n = 2048, k = 1025, activeBands = numBands;
    double statKeep = 0.0, alphaKeep = 0.0;
    Params params;
    Velvet::Sequence seq;
    DelayLine line;
    std::array<int, numBands + 1> edges {};
    std::array<double, numBands> centres {};
    std::array<double, numBands> pm {}, pd {}, xr {};
    std::array<float, numBands> alpha {}, betas {};
    std::array<std::atomic<float>, numBands> dispTarget {}, dispAchieved {};
};
} // namespace sph
