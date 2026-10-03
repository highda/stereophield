#include "dsp/VirtualListener.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <numbers>

namespace sph
{
namespace
{
constexpr double headRadius = 0.0875, soundSpeed = 343.0;

// Head-shadow filter of Brown and Duda for a source at angle theta (radians)
// from the ear axis, at angular frequency w.
std::complex<double> shadow (double theta, double w)
{
    const double w0 = soundSpeed / headRadius;
    const double alphaMin = 0.1, thetaMin = 150.0 * std::numbers::pi / 180.0;
    const double alpha = (1.0 + alphaMin / 2.0) + (1.0 - alphaMin / 2.0) * std::cos (theta / thetaMin * std::numbers::pi);
    return std::complex<double> (1.0, alpha * w / (2.0 * w0)) / std::complex<double> (1.0, w / (2.0 * w0));
}

// Woodworth's delay of an ear for a source at angle theta from the ear axis.
double delay (double theta)
{
    const double a = headRadius / soundSpeed;
    return theta < std::numbers::pi / 2.0 ? -a * std::cos (theta) : a * (theta - std::numbers::pi / 2.0);
}

double erbNumber (double f) { return 21.4 * std::log10 (1.0 + 0.00437 * f); }
double erbFrequency (double e) { return (std::pow (10.0, e / 21.4) - 1.0) / 0.00437; }
} // namespace

VirtualListener::VirtualListener() = default;
VirtualListener::~VirtualListener() = default;

double VirtualListener::bandCentre (int band) noexcept
{
    const double lo = erbNumber (150.0), hi = erbNumber (8000.0);
    return erbFrequency (lo + (hi - lo) * (band + 0.5) / numBands);
}

void VirtualListener::prepare (double sampleRate, double smoothingSeconds, Playback playback)
{
    fs = sampleRate;
    win = (int) std::lround (0.1 * fs);
    order = 1;
    while ((1 << order) < 2 * win)
        ++order;
    n = 1 << order;
    fft = std::make_unique<juce::dsp::FFT> (order);
    bufL.assign ((size_t) (2 * n), 0.0f);
    bufR.assign ((size_t) (2 * n), 0.0f);
    cross.assign ((size_t) (2 * n), 0.0f);
    const int k = n / 2 + 1;
    earL.assign ((size_t) k, {});
    earR.assign ((size_t) k, {});
    hIpsi.resize ((size_t) k);
    hContra.resize ((size_t) k);

    // Loudspeakers at +-30 degrees from the front; ears on the interaural axis.
    // The near loudspeaker is 60 degrees from an ear's axis, the far one 120.
    const double near = 60.0 * std::numbers::pi / 180.0, far = 120.0 * std::numbers::pi / 180.0;
    for (int b = 0; b < k; ++b)
    {
        const double w = 2.0 * std::numbers::pi * b * fs / n;
        if (playback == Playback::Headphones)
        {
            hIpsi[(size_t) b] = 1.0;
            hContra[(size_t) b] = 0.0;
            continue;
        }
        hIpsi[(size_t) b] = shadow (near, w) * std::polar (1.0, -w * delay (near));
        hContra[(size_t) b] = shadow (far, w) * std::polar (1.0, -w * delay (far));
    }
    const double lo = erbNumber (150.0), hi = erbNumber (8000.0);
    for (int e = 0; e < numBands; ++e)
    {
        edges[(size_t) e] = std::max (1, (int) std::lround (erbFrequency (lo + (hi - lo) * e / numBands) * n / fs));
        hiEdge[(size_t) e] = std::max (1, (int) std::lround (erbFrequency (lo + (hi - lo) * (e + 1) / numBands) * n / fs));
    }
    for (int o = 0; o < 3; ++o)
    {
        const double fc = 500.0 * (1 << o);
        edges[(size_t) (numBands + o)] = (int) std::lround (fc / std::sqrt (2.0) * n / fs);
        hiEdge[(size_t) (numBands + o)] = (int) std::lround (fc * std::sqrt (2.0) * n / fs);
    }
    keep = (float) std::exp (-0.1 / smoothingSeconds);
    for (int b = 0; b < allBands; ++b)
    {
        const size_t len = (size_t) (hiEdge[(size_t) b] - edges[(size_t) b] + 1);
        sxx[(size_t) b].assign (len, 0.0);
        syy[(size_t) b].assign (len, 0.0);
        sxy[(size_t) b].assign (len, {});
    }
    reset();
}

void VirtualListener::reset()
{
    for (int b = 0; b < allBands; ++b)
    {
        std::fill (sxx[(size_t) b].begin(), sxx[(size_t) b].end(), 0.0);
        std::fill (syy[(size_t) b].begin(), syy[(size_t) b].end(), 0.0);
        std::fill (sxy[(size_t) b].begin(), sxy[(size_t) b].end(), std::complex<double> {});
    }
    iaccs.fill (1.0f);
    e3.fill (1.0f);
    width = 0.0f;
    first = true;
}

void VirtualListener::addWindow (const float* l, const float* r)
{
    for (int i = 0; i < n; ++i)
    {
        const float w = i < win ? (float) (0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * i / win)) : 0.0f;
        bufL[(size_t) i] = i < win ? l[i] * w : 0.0f;
        bufR[(size_t) i] = i < win ? r[i] * w : 0.0f;
    }
    std::fill (bufL.begin() + n, bufL.end(), 0.0f);
    std::fill (bufR.begin() + n, bufR.end(), 0.0f);
    fft->performRealOnlyForwardTransform (bufL.data(), true);
    fft->performRealOnlyForwardTransform (bufR.data(), true);
    const int k = n / 2 + 1;
    for (int b = 0; b < k; ++b)
    {
        const std::complex<double> L (bufL[(size_t) (2 * b)], bufL[(size_t) (2 * b + 1)]);
        const std::complex<double> R (bufR[(size_t) (2 * b)], bufR[(size_t) (2 * b + 1)]);
        earL[(size_t) b] = hIpsi[(size_t) b] * L + hContra[(size_t) b] * R;
        earR[(size_t) b] = hIpsi[(size_t) b] * R + hContra[(size_t) b] * L;
    }

    const double a = first ? 0.0 : keep, c = 1.0 - a;
    first = false;
    const int maxLag = (int) std::lround (0.001 * fs);
    for (int band = 0; band < allBands; ++band)
    {
        const int k0 = edges[(size_t) band], k1 = hiEdge[(size_t) band];
        double ex = 0, ey = 0;
        std::fill (cross.begin(), cross.end(), 0.0f);
        for (int b = k0; b <= k1; ++b)
        {
            auto& xx = sxx[(size_t) band][(size_t) (b - k0)];
            auto& yy = syy[(size_t) band][(size_t) (b - k0)];
            auto& xy = sxy[(size_t) band][(size_t) (b - k0)];
            xx = a * xx + c * std::norm (earL[(size_t) b]);
            yy = a * yy + c * std::norm (earR[(size_t) b]);
            xy = a * xy + c * (earL[(size_t) b] * std::conj (earR[(size_t) b]));
            ex += xx;
            ey += yy;
            cross[(size_t) (2 * b)] = (float) xy.real();
            cross[(size_t) (2 * b + 1)] = (float) xy.imag();
        }
        // Band-limited cross-correlation; the inverse transform's 1/N and
        // the one-sided spectrum give the scale 2 / N relative to ex.
        fft->performRealOnlyInverseTransform (cross.data());
        double best = 0.0;
        for (int lag = -maxLag; lag <= maxLag; ++lag)
            best = std::max (best, std::abs ((double) cross[(size_t) ((lag + n) % n)]));
        const double norm = std::sqrt (ex * ey) / n * 2.0;
        const float v = norm > 1e-30 ? (float) std::min (1.0, best / norm) : 1.0f;
        if (band < numBands)
            iaccs[(size_t) band] = v;
        else
            e3[(size_t) (band - numBands)] = v;
    }
    width = 1.0f - (e3[0] + e3[1] + e3[2]) / 3.0f;
}
} // namespace sph
