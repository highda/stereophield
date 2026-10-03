#pragma once

// Spectral and statistical helpers for measurements.

#include "signals/TestSignals.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <complex>
#include <vector>

namespace sph::test
{
using signals::Signal;

// Complex spectrum (bins 0 .. N/2) of x zero-padded or truncated to 2^order.
inline std::vector<std::complex<double>> spectrum (const float* x, size_t len, int order)
{
    const int n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf ((size_t) (2 * n), 0.0f);
    std::copy (x, x + std::min (len, (size_t) n), buf.begin());
    fft.performRealOnlyForwardTransform (buf.data(), true);
    std::vector<std::complex<double>> out ((size_t) (n / 2 + 1));
    for (int k = 0; k <= n / 2; ++k)
        out[(size_t) k] = { buf[(size_t) (2 * k)], buf[(size_t) (2 * k + 1)] };
    return out;
}

inline double pearson (const std::vector<double>& a, const std::vector<double>& b)
{
    const size_t n = a.size();
    double ma = 0, mb = 0;
    for (size_t i = 0; i < n; ++i)
    {
        ma += a[i];
        mb += b[i];
    }
    ma /= (double) n;
    mb /= (double) n;
    double sab = 0, saa = 0, sbb = 0;
    for (size_t i = 0; i < n; ++i)
    {
        sab += (a[i] - ma) * (b[i] - mb);
        saa += (a[i] - ma) * (a[i] - ma);
        sbb += (b[i] - mb) * (b[i] - mb);
    }
    return sab / std::sqrt (saa * sbb + 1e-300);
}

// Zero-lag normalised correlation of x and y over [from, to).
inline double correlation (const Signal& x, const Signal& y, size_t from, size_t to)
{
    double xy = 0, xx = 0, yy = 0;
    for (size_t i = from; i < to; ++i)
    {
        xy += (double) x[i] * y[i];
        xx += (double) x[i] * x[i];
        yy += (double) y[i] * y[i];
    }
    const double d = std::sqrt (xx * yy);
    return d < 1e-30 ? 0.0 : xy / d;
}

// 10 log10 of energy(a - b) / energy(ref) over [from, to), where b is
// scaled by gainB and delayed by delayB samples.
inline double errorDb (const Signal& a, const Signal& b, double gainB, int delayB, const Signal& ref,
                       size_t from, size_t to)
{
    double e = 0, r = 0;
    for (size_t i = from; i < to; ++i)
    {
        const double bv = (long) i - delayB >= 0 ? gainB * b[i - (size_t) delayB] : 0.0;
        const double d = (double) a[i] - bv;
        e += d * d;
        r += (double) ref[i] * ref[i];
    }
    return 10.0 * std::log10 (std::max (e, 1e-300) / std::max (r, 1e-300));
}

inline bool allFinite (const Signal& x)
{
    for (float v : x)
        if (! std::isfinite (v))
            return false;
    return true;
}
} // namespace sph::test
