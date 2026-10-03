#include "dsp/ImageExpander.h"

#include <algorithm>
#include <cmath>

namespace sph
{
void ImageExpander::prepare (double sampleRate, int fftSize, int hop)
{
    fs = sampleRate;
    n = fftSize;
    k = n / 2 + 1;
    keep = std::exp (-(double) hop / (0.050 * fs));
    mm.assign ((size_t) k, 0.0);
    ss.assign ((size_t) k, 0.0);
    sm.assign ((size_t) k, {});
}

void ImageExpander::reset()
{
    std::fill (mm.begin(), mm.end(), 0.0);
    std::fill (ss.begin(), ss.end(), 0.0);
    std::fill (sm.begin(), sm.end(), std::complex<double> {});
}

double ImageExpander::knee (double v) noexcept
{
    // Identity up to 0.9, then a tanh knee that approaches 1.
    const double a = std::abs (v);
    const double y = a <= 0.9 ? a : 0.9 + 0.1 * std::tanh ((a - 0.9) / 0.1);
    return v < 0.0 ? -y : y;
}

void ImageExpander::processFrame (const std::complex<float>* x, const std::complex<float>* sIn, std::complex<float>* sOut) noexcept
{
    const int kc = (int) std::ceil (centreHz * n / fs);
    const double c = 1.0 - keep;
    for (int b = 0; b < k; ++b)
    {
        const std::complex<double> M (x[b]), S (sIn[b]);
        mm[(size_t) b] = keep * mm[(size_t) b] + c * std::norm (M);
        ss[(size_t) b] = keep * ss[(size_t) b] + c * std::norm (S);
        sm[(size_t) b] = keep * sm[(size_t) b] + c * (S * std::conj (M));
        if (b < kc || mm[(size_t) b] < 1e-20)
        {
            sOut[b] = sIn[b];
            continue;
        }
        const double rho = std::clamp (sm[(size_t) b].real() / mm[(size_t) b], -1.0, 1.0);
        const double kappa = std::min (1.0, std::abs (sm[(size_t) b]) / std::sqrt (mm[(size_t) b] * ss[(size_t) b] + 1e-30));
        const double rho2 = amount == 1.0f ? rho : knee (rho * amount);
        const std::complex<double> out = S + kappa * (rho2 - rho) * M + (1.0 - kappa) * (diffuse - 1.0) * S;
        sOut[b] = std::complex<float> ((float) out.real(), (float) out.imag());
    }
}
} // namespace sph
