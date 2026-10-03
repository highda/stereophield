#include "dsp/Loudness.h"

#include "dsp/Biquad.h"

#include <cmath>
#include <numbers>
#include <vector>

namespace sph
{
namespace
{
// The two K-weighting stages, designed by the bilinear transform from their
// analogue prototypes so that they match BS.1770's 48 kHz coefficients and
// work at any rate.
std::array<double, 6> shelf (double fs)
{
    const double f0 = 1681.974450955533, g = 3.999843853973347, q = 0.7071752369554196;
    const double k = std::tan (std::numbers::pi * f0 / fs);
    const double vh = std::pow (10.0, g / 20.0), vb = std::pow (vh, 0.4996667741545416);
    const double a0 = 1.0 + k / q + k * k;
    return { (vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0,
             1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
}

std::array<double, 6> highPass (double fs)
{
    const double f0 = 38.13547087602444, q = 0.5003270373238773;
    const double k = std::tan (std::numbers::pi * f0 / fs);
    const double a0 = 1.0 + k / q + k * k;
    return { 1.0, -2.0, 1.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
}

std::vector<double> weightedSquares (const float* x, size_t n, double fs)
{
    Biquad a, b;
    a.setCoefficients (shelf (fs));
    b.setCoefficients (highPass (fs));
    std::vector<double> y (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double v = b.process (a.process (x[i]));
        y[i] = v * v;
    }
    return y;
}
} // namespace

double Loudness::integrated (const float* l, const float* r, size_t n, double fs)
{
    const auto zl = weightedSquares (l, n, fs);
    const auto zr = r != nullptr ? weightedSquares (r, n, fs) : std::vector<double> (n, 0.0);
    const size_t block = (size_t) std::lround (0.4 * fs), step = block / 4;
    std::vector<double> powers;
    for (size_t s = 0; s + block <= n; s += step)
    {
        double p = 0.0;
        for (size_t i = s; i < s + block; ++i)
            p += zl[i] + zr[i];
        powers.push_back (p / (double) block);
    }
    auto loud = [] (double p) { return -0.691 + 10.0 * std::log10 (std::max (p, 1e-30)); };
    auto gatedMean = [&] (double threshold)
    {
        double sum = 0.0;
        int count = 0;
        for (double p : powers)
            if (loud (p) > threshold)
            {
                sum += p;
                ++count;
            }
        return count > 0 ? sum / count : 0.0;
    };
    const double abs = gatedMean (-70.0);
    if (abs <= 0.0)
        return -200.0;
    const double rel = gatedMean (loud (abs) - 10.0);
    return rel > 0.0 ? loud (rel) : -200.0;
}
} // namespace sph
