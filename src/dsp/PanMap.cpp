#include "dsp/PanMap.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sph
{
double PanMap::displayFrequency (int i) noexcept
{
    return 40.0 * std::pow (16000.0 / 40.0, (double) i / (displayPoints - 1));
}

void PanMap::prepare (double sampleRate, int fftSize, int hop)
{
    fs = sampleRate;
    n = fftSize;
    k = n / 2 + 1;
    timeCoeff = std::exp (-(double) hop / (0.030 * fs));
    ownRadius = 6 * n / 2048;
    partials.prepare (fs, n);
    sources.prepare (fs, hop);
    for (auto* v : { &at, &target, &p, &pSmooth })
        v->assign ((size_t) k, 0.0f);
    weight.assign ((size_t) k, 1.0f);
    for (int i = 0; i < displayPoints; ++i)
        dispBin[(size_t) i] = std::clamp ((int) std::lround (displayFrequency (i) * n / fs), 0, k - 1);
    reset();
}

void PanMap::reset()
{
    partials.reset();
    sources.reset();
    std::fill (p.begin(), p.end(), 0.0f);
    std::fill (pSmooth.begin(), pSmooth.end(), 0.0f);
    frameIndex = 0;
    for (int i = 0; i < displayPoints; ++i)
    {
        dispPan[(size_t) i].store (0.0f, std::memory_order_relaxed);
        dispMag[(size_t) i].store (0.0f, std::memory_order_relaxed);
    }
}

double PanMap::curve (double f) const noexcept
{
    if (f < bassCentre)
        return 0.0;
    return std::sin (2.0 * std::numbers::pi * panDensity * std::log2 (f / 100.0));
}

void PanMap::processFrame (const std::complex<float>* x, const float* a, const float* mt2, const float* mn2,
                           std::complex<float>* s) noexcept
{
    // Target pan per bin.
    if (panMode == PanMode::Static)
    {
        for (int b = 0; b < k; ++b)
            target[(size_t) b] = (float) curve ((double) b * fs / n);
    }
    else
    {
        for (int b = 0; b < k; ++b)
            at[(size_t) b] = mt2[b] * a[b];
        partials.process (at.data(), k, frameIndex);
        auto& tracks = partials.tracks();
        if (panMode == PanMode::Groups)
            sources.process (tracks, frameIndex, groupLimit, bassCentre);

        std::fill (target.begin(), target.end(), 0.0f);

        // Bin ownership: each track of age >= 1 owns the bins nearer to its
        // peak than to any other such track's peak, within +-6 N/2048 bins.
        int count = 0;
        for (int i = 0; i < PartialTracker::maxTracks; ++i)
            if (tracks[(size_t) i].active && tracks[(size_t) i].age >= 1)
                owners[(size_t) count++] = i;
        std::sort (owners.begin(), owners.begin() + count, [&] (int l, int r)
        {
            return tracks[(size_t) l].peakBin != tracks[(size_t) r].peakBin ? tracks[(size_t) l].peakBin < tracks[(size_t) r].peakBin : l < r;
        });
        for (int o = 0; o < count; ++o)
        {
            const auto& t = tracks[(size_t) owners[(size_t) o]];
            const int pb = t.peakBin;
            int lo = pb - ownRadius, hi = pb + ownRadius;
            if (o > 0)
            {
                const int prev = tracks[(size_t) owners[(size_t) (o - 1)]].peakBin;
                lo = std::max (lo, (prev + pb) / 2 + 1);
            }
            if (o + 1 < count)
            {
                const int next = tracks[(size_t) owners[(size_t) (o + 1)]].peakBin;
                hi = std::min (hi, (pb + next) / 2);
            }
            const double pan = panMode == PanMode::Tracks ? curve (t.birthFreq) : t.pan;
            for (int b = std::max (0, lo); b <= std::min (k - 1, hi); ++b)
                target[(size_t) b] = (float) pan;
        }
    }

    // Smoothing in time, then across frequency with [0.25, 0.5, 0.25].
    const float ca = (float) timeCoeff;
    for (int b = 0; b < k; ++b)
        p[(size_t) b] = ca * p[(size_t) b] + (1.0f - ca) * target[(size_t) b];
    for (int b = 0; b < k; ++b)
    {
        const float l = p[(size_t) std::max (0, b - 1)], r = p[(size_t) std::min (k - 1, b + 1)];
        float v = 0.25f * l + 0.5f * p[(size_t) b] + 0.25f * r;
        pSmooth[(size_t) b] = std::abs (v) < 1.0e-20f ? 0.0f : v;
    }
    for (int b = 0; b < k; ++b)
        if (std::abs (p[(size_t) b]) < 1.0e-20f)
            p[(size_t) b] = 0.0f;

    // Side spectrum.
    const float depth = (float) panDepth;
    const bool all = panMode == PanMode::Static;
    for (int b = 0; b < k; ++b)
    {
        const float mask = all ? mt2[b] + mn2[b] : mt2[b];
        s[b] = x[b] * (depth * pSmooth[(size_t) b] * mask * weight[(size_t) b]);
    }

    for (int i = 0; i < displayPoints; ++i)
    {
        const int b = dispBin[(size_t) i];
        dispPan[(size_t) i].store (pSmooth[(size_t) b], std::memory_order_relaxed);
        dispMag[(size_t) i].store (a[b] * (all ? mt2[b] + mn2[b] : mt2[b]), std::memory_order_relaxed);
    }
    ++frameIndex;
}
} // namespace sph
