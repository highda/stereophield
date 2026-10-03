#include "dsp/CoherenceDesigner.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sph
{
namespace
{
double erbNumber (double f) { return 21.4 * std::log10 (1.0 + 0.00437 * f); }
double erbFrequency (double e) { return (std::pow (10.0, e / 21.4) - 1.0) / 0.00437; }

// Monotone cubic interpolation (Fritsch-Carlson) through points (xs, ys).
double monotoneCubic (const double* xs, const double* ys, int count, double x)
{
    if (x <= xs[0])
        return ys[0];
    if (x >= xs[count - 1])
        return ys[count - 1];
    double d[8], m[8];
    for (int i = 0; i + 1 < count; ++i)
        d[i] = (ys[i + 1] - ys[i]) / (xs[i + 1] - xs[i]);
    m[0] = d[0];
    m[count - 1] = d[count - 2];
    for (int i = 1; i + 1 < count; ++i)
        m[i] = d[i - 1] * d[i] <= 0.0 ? 0.0 : 0.5 * (d[i - 1] + d[i]);
    for (int i = 0; i + 1 < count; ++i)
        if (d[i] == 0.0)
            m[i] = m[i + 1] = 0.0;
        else
        {
            const double a = m[i] / d[i], b = m[i + 1] / d[i], h = a * a + b * b;
            if (h > 9.0)
            {
                const double t = 3.0 / std::sqrt (h);
                m[i] = t * a * d[i];
                m[i + 1] = t * b * d[i];
            }
        }
    int i = 0;
    while (x > xs[i + 1])
        ++i;
    const double h = xs[i + 1] - xs[i], t = (x - xs[i]) / h;
    const double t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * ys[i] + (t3 - 2 * t2 + t) * h * m[i] + (-2 * t3 + 3 * t2) * ys[i + 1]
           + (t3 - t2) * h * m[i + 1];
}

double patternA (MicPattern p)
{
    switch (p)
    {
        case MicPattern::Omni: return 1.0;
        case MicPattern::Subcardioid: return 0.7;
        case MicPattern::Cardioid: return 0.5;
        case MicPattern::Supercardioid: return 0.37;
        case MicPattern::Figure8: return 0.0;
    }
    return 0.5;
}
} // namespace

double CoherenceDesigner::bandCentre (int b, double sampleRate) noexcept
{
    const double lo = erbNumber (50.0), hi = erbNumber (std::min (20000.0, 0.45 * sampleRate));
    return erbFrequency (lo + (hi - lo) * (b + 0.5) / numBands);
}

int CoherenceDesigner::layout (double sampleRate, int fftSize, int* edgeBins, double* centresHz) noexcept
{
    const double lo = erbNumber (50.0), hi = erbNumber (std::min (20000.0, 0.45 * sampleRate));
    const int k = fftSize / 2 + 1;
    int count = 0;
    int start = std::clamp ((int) std::lround (erbFrequency (lo) * fftSize / sampleRate), 1, k - 1);
    edgeBins[0] = start;
    for (int e = 1; e <= numBands; ++e)
    {
        const int edge = std::clamp ((int) std::lround (erbFrequency (lo + (hi - lo) * e / numBands) * fftSize / sampleRate), 1, k - 1);
        if (edge - start >= 3 || (e == numBands && count == 0))
        {
            edgeBins[++count] = edge;
            start = edge;
        }
        else if (e == numBands)
            edgeBins[count] = edge; // fold a short remainder into the last band
    }
    for (int b = 0; b < count; ++b)
        centresHz[b] = std::sqrt ((double) edgeBins[b] * edgeBins[b + 1]) * sampleRate / fftSize;
    return count;
}

void CoherenceDesigner::prepare (double sampleRate, int fftSize, int hop)
{
    fs = sampleRate;
    n = fftSize;
    k = n / 2 + 1;
    statKeep = std::exp (-(double) hop / (0.100 * fs));
    alphaKeep = std::exp (-(double) hop / (0.030 * fs));
    // A fixed 20 ms optimised velvet sequence.
    Velvet::buildOptimised (seq, fs, 20.0f, 1000.0f, 7, 0);
    line.prepare ((int) std::ceil (0.021 * fs));
    activeBands = layout (fs, n, edges.data(), centres.data());
    reset();
}

void CoherenceDesigner::reset()
{
    line.reset();
    pm.fill (0.0);
    pd.fill (0.0);
    xr.fill (0.0);
    alpha.fill (0.0f);
    betas.fill (0.0f);
}

void CoherenceDesigner::setParams (const Params& p) noexcept
{
    params = p;
}

double CoherenceDesigner::targetFor (const Params& params, double f) noexcept
{
    double c = 1.0;
    const double speed = 343.0;
    const double d = params.cohSpacingCm * 0.01;
    auto spaced = [&] { const double x = 2.0 * std::numbers::pi * f * d / speed; return x < 1e-9 ? 1.0 : std::sin (x) / x; };
    auto coincident = [&]
    {
        const double a = patternA (params.cohPattern), b = 1.0 - a;
        const double phi = params.cohAngleDeg * std::numbers::pi / 180.0;
        return (a * a + b * b * std::cos (phi) / 3.0) / (a * a + b * b / 3.0);
    };
    switch (params.cohMode)
    {
        case CohMode::Curve:
        {
            static constexpr double xs[5] = { 5.977, 7.966, 9.966, 11.966, 13.966 }; // log2 of 63 .. 16000 Hz
            double ys[5];
            for (int i = 0; i < 5; ++i)
                ys[i] = params.cohPoints[i];
            c = monotoneCubic (xs, ys, 5, std::log2 (std::max (f, 1.0)));
            break;
        }
        case CohMode::SpacedPair: c = spaced(); break;
        case CohMode::CoincidentPair: c = coincident(); break;
        case CohMode::NearCoincident: c = spaced() * coincident(); break;
    }
    return std::clamp (c, -0.95, 1.0);
}

float CoherenceDesigner::decorrelate (float m) noexcept
{
    line.push (m);
    float v = 0.0f;
    for (int j = 0; j < seq.count; ++j)
        v += seq.gain[(size_t) j] * line.readInt (seq.pos[(size_t) j]);
    return v;
}

void CoherenceDesigner::processFrame (const std::complex<float>* x, const std::complex<float>* d, const float* mt2,
                                      const float* mn2, const float* mx, std::complex<float>* s) noexcept
{
    auto maskOf = [&] (int b) -> float
    {
        switch (params.cohSource)
        {
            case Source::Tonal: return mt2[b];
            case Source::Noise: return mn2[b];
            case Source::TonalNoise: return mt2[b] + mn2[b];
            case Source::Full: break;
        }
        return 1.0f;
    };

    // Per band: smoothed powers of M and D and their real cross term. The
    // part of D in phase with M is removed (D' = D - beta M with
    // beta = XR / PM), so D' is uncorrelated with M (for a steady tone it is
    // in quadrature) and every target coherence is reachable:
    //   ICC = (PM - a^2 PD') / (PM + a^2 PD')   =>   a = sqrt((1 - c) / (1 + c) PM / PD').
    for (int band = 0; band < activeBands; ++band)
    {
        double sm = 0, sd = 0, sx = 0;
        const int k0 = edges[(size_t) band], k1 = std::max (k0 + 1, edges[(size_t) band + 1]);
        for (int b = k0; b < k1; ++b)
        {
            const float w = maskOf (b);
            const std::complex<double> mb (x[b].real() * w, x[b].imag() * w);
            const std::complex<double> db (d[b].real() * w, d[b].imag() * w);
            sm += std::norm (mb);
            sd += std::norm (db);
            sx += (mb * std::conj (db)).real();
        }
        auto& PM = pm[(size_t) band];
        auto& PD = pd[(size_t) band];
        auto& XR = xr[(size_t) band];
        PM = statKeep * PM + (1.0 - statKeep) * sm;
        PD = statKeep * PD + (1.0 - statKeep) * sd;
        XR = statKeep * XR + (1.0 - statKeep) * sx;

        const double c = target (centres[(size_t) band]);
        const double beta = PM > 1e-20 ? XR / PM : 0.0;
        const double pdOrth = std::max (0.0, PD - XR * beta);
        double a = 0.0;
        if (PM > 1e-20 && pdOrth > 1e-12 * PM)
            a = std::min (alphaMax, std::sqrt ((1.0 - c) / (1.0 + c) * PM / pdOrth));
        alpha[(size_t) band] = (float) (alphaKeep * alpha[(size_t) band] + (1.0 - alphaKeep) * a);
        betas[(size_t) band] = (float) beta;
        const double a2 = (double) alpha[(size_t) band] * alpha[(size_t) band];
        dispTarget[(size_t) band].store ((float) c, std::memory_order_relaxed);
        dispAchieved[(size_t) band].store ((float) ((PM - a2 * pdOrth) / (PM + a2 * pdOrth + 1e-30)), std::memory_order_relaxed);

        for (int b = k0; b < k1; ++b)
        {
            const float w = maskOf (b);
            // Only clearly transient bins are protected: stage A's transient
            // mask hovers around 0.3 on steady noise.
            const double tr = std::clamp ((mx[b] - 0.4) / 0.6, 0.0, 1.0);
            const float g = (float) (alpha[(size_t) band] * (1.0 - params.cohTransient * tr * tr) * w);
            s[b] = g * (d[b] - (float) beta * x[b]);
        }
    }
    // Bins outside the band edges carry no side.
    for (int b = 0; b < edges[0]; ++b)
        s[b] = {};
    for (int b = std::max (edges[0] + 1, edges[(size_t) activeBands]); b < k; ++b)
        s[b] = {};
}
} // namespace sph
