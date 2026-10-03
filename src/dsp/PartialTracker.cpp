#include "dsp/PartialTracker.h"

#include <algorithm>
#include <cmath>

namespace sph
{
void PartialTracker::prepare (double sampleRate, int fftSize)
{
    fs = sampleRate;
    n = fftSize;
    reset();
}

void PartialTracker::reset()
{
    for (auto& t : slots)
        t = Track {};
    numPeaks = 0;
}

int PartialTracker::numActive() const noexcept
{
    int c = 0;
    for (const auto& t : slots)
        c += t.active ? 1 : 0;
    return c;
}

void PartialTracker::process (const float* at, int numBins, int frame) noexcept
{
    // 1. Peak picking.
    float maxAt = 0.0f;
    for (int k = 0; k < numBins; ++k)
        maxAt = std::max (maxAt, at[k]);
    const float absFloor = 3.16227766e-4f;          // -70 dB
    const float relFloor = 3.16227766e-3f * maxAt;  // -50 dB below the maximum
    const int kLo = std::max (1, (int) std::ceil (40.0 * n / fs));
    const int kHi = std::min (numBins - 2, (int) std::floor (12000.0 * n / fs));

    numPeaks = 0;
    int weakest = -1;
    for (int k = kLo; k <= kHi; ++k)
    {
        const float a = at[k];
        if (! (a > at[k - 1] && a >= at[k + 1] && a > absFloor && a > relFloor))
            continue;
        // 2. Parabolic refinement on log magnitude.
        const double la = std::log ((double) at[k - 1] + 1e-30), lb = std::log ((double) a),
                     lc = std::log ((double) at[k + 1] + 1e-30);
        const double den = la - 2.0 * lb + lc;
        const double delta = den != 0.0 ? std::clamp (0.5 * (la - lc) / den, -0.5, 0.5) : 0.0;
        const Peak p { (k + delta) * fs / n, (double) a, k, false };
        // Keep the 60 largest.
        if (numPeaks < maxPeaks)
        {
            peaks[(size_t) numPeaks++] = p;
            if (numPeaks == maxPeaks)
                weakest = (int) (std::min_element (peaks.begin(), peaks.end(), [] (auto& x, auto& y) { return x.amp < y.amp; }) - peaks.begin());
        }
        else if (p.amp > peaks[(size_t) weakest].amp)
        {
            peaks[(size_t) weakest] = p;
            weakest = (int) (std::min_element (peaks.begin(), peaks.end(), [] (auto& x, auto& y) { return x.amp < y.amp; }) - peaks.begin());
        }
    }

    // 3. Matching, live tracks in order of decreasing amplitude.
    int live = 0;
    for (int i = 0; i < maxTracks; ++i)
        if (slots[(size_t) i].active)
            order[(size_t) live++] = i;
    std::sort (order.begin(), order.begin() + live, [&] (int a, int b)
    {
        return slots[(size_t) a].amp != slots[(size_t) b].amp ? slots[(size_t) a].amp > slots[(size_t) b].amp : a < b;
    });
    for (int o = 0; o < live; ++o)
    {
        auto& t = slots[(size_t) order[(size_t) o]];
        int best = -1;
        double bestDist = 1e300;
        for (int p = 0; p < numPeaks; ++p)
        {
            const auto& pk = peaks[(size_t) p];
            if (pk.claimed || std::abs (pk.freq / t.freq - 1.0) > matchTolerance)
                continue;
            const double dist = std::abs (pk.freq - t.freq);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = p;
            }
        }
        if (best >= 0)
        {
            auto& pk = peaks[(size_t) best];
            pk.claimed = true;
            t.freq = 0.5 * t.freq + 0.5 * pk.freq;
            t.amp = pk.amp;
            t.peakBin = pk.bin;
            t.missed = 0;
            t.age += 1;
        }
        else
        {
            // 4. Death after more than three missed frames. A missed track
            // has no measured amplitude this frame.
            t.amp = 0.0;
            t.peakBin = (int) std::lround (t.freq * n / fs);
            if (++t.missed > 3)
                t = Track {};
        }
    }

    // 5. Birth.
    int slot = 0;
    for (int p = 0; p < numPeaks; ++p)
    {
        const auto& pk = peaks[(size_t) p];
        if (pk.claimed)
            continue;
        while (slot < maxTracks && slots[(size_t) slot].active)
            ++slot;
        if (slot == maxTracks)
            break;
        auto& t = slots[(size_t) slot];
        t = Track {};
        t.active = true;
        t.freq = t.birthFreq = pk.freq;
        t.amp = pk.amp;
        t.birthFrame = frame;
        t.peakBin = pk.bin;
    }
}
} // namespace sph
