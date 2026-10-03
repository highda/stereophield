#pragma once

#include "dsp/Params.h"

#include <complex>
#include <vector>

namespace sph
{
// Frequency-domain image expander for stereo input (PART2_LEDGER.md X1,
// after Avendano and Jot). Per bin, from smoothed statistics of the input mid
// M and side S: the panning index rho = Re E[S M*] / E|M|^2 and the
// coherence kappa = |E[S M*]| / sqrt(E|S|^2 E|M|^2) (1 for one panned source,
// near 0 for diffuse sound). Panned content moves to rho' = clip(a rho) with a
// soft knee at the loudspeaker; diffuse content is scaled by d:
//   S' = S + kappa (rho' - rho) M + (1 - kappa) (d - 1) S
// which is exactly S at a = d = 1. The mid is never touched.
class ImageExpander
{
public:
    void prepare (double sampleRate, int fftSize, int hop);
    void reset();
    void setParams (const Params& p) noexcept { amount = p.imgAmount; diffuse = p.imgDiffuse; centreHz = p.imgCenterHz; }
    bool isNeutral() const noexcept { return amount == 1.0f && diffuse == 1.0f; }

    // x: mid spectrum; sIn: side spectrum. Writes the new side spectrum.
    void processFrame (const std::complex<float>* x, const std::complex<float>* sIn, std::complex<float>* sOut) noexcept;

    static double knee (double v) noexcept;

private:
    double fs = 48000.0, keep = 0.0;
    int n = 2048, k = 1025;
    float amount = 1.0f, diffuse = 1.0f, centreHz = 120.0f;
    std::vector<double> mm, ss;
    std::vector<std::complex<double>> sm;
};
} // namespace sph
