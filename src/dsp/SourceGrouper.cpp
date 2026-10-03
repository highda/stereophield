#include "dsp/SourceGrouper.h"

#include <algorithm>
#include <cmath>

namespace sph
{
void SourceGrouper::prepare (double sampleRate, int hop)
{
    memoryFrames = std::max (1, (int) std::lround (memorySeconds * sampleRate / hop));
    reset();
}

void SourceGrouper::reset()
{
    for (auto& g : slots)
        g = Group {};
    for (auto& m : history)
        m = Memory {};
    historyPos = 0;
}

int SourceGrouper::numActive() const noexcept
{
    int c = 0;
    for (const auto& g : slots)
        c += g.active ? 1 : 0;
    return c;
}

double SourceGrouper::assignPan (double f0, int frame, double bassCentreHz, int exclude) const noexcept
{
    // 1. Bass stays in the centre.
    if (f0 < bassCentreHz)
        return 0.0;

    // 2. Voice continuity: a recently ended or fading group within 7
    // semitones lends its position, nearest in pitch first.
    double bestDist = 1e300, bestPan = 0.0;
    auto consider = [&] (double f, double pan)
    {
        const double semis = std::abs (12.0 * std::log2 (f0 / f));
        if (semis <= 7.0 && semis < bestDist)
        {
            bestDist = semis;
            bestPan = pan;
        }
    };
    for (const auto& m : history)
        if (m.valid && frame - m.diedFrame <= memoryFrames)
            consider (m.f0, m.pan);
    for (int gi = 0; gi < maxGroups; ++gi)
    {
        const auto& g = slots[(size_t) gi];
        if (gi != exclude && g.active && g.amp < fadingFactor * g.peakAmp)
            consider (g.f0, g.pan);
    }
    if (bestDist < 1e300)
        return bestPan;

    // 3. The free slot farthest from every active group.
    static constexpr double slotList[] = { -0.5, 0.5, -1.0, 1.0, -0.25, 0.25, -0.75, 0.75 };
    double best = slotList[0], bestSpace = -1.0;
    for (double s : slotList)
    {
        double space = 1e300;
        for (int gi = 0; gi < maxGroups; ++gi)
            if (gi != exclude && slots[(size_t) gi].active)
                space = std::min (space, std::abs (s - slots[(size_t) gi].pan));
        if (space > bestSpace)
        {
            bestSpace = space;
            best = s;
        }
    }
    return best;
}

void SourceGrouper::process (std::array<PartialTracker::Track, PartialTracker::maxTracks>& tracks, int frame,
                             int maxActiveGroups, double bassCentreHz) noexcept
{
    // Group amplitudes from their live tracks; tracks of a freed group or
    // dead tracks no longer count.
    for (auto& g : slots)
    {
        g.amp = 0.0;
        g.numTracks = 0;
    }
    for (auto& t : tracks)
        if (t.active && t.group >= 0)
        {
            auto& g = slots[(size_t) t.group];
            if (! g.active)
            {
                t.group = -1;
                t.pan = 0.0;
                continue;
            }
            g.amp += t.amp;
            ++g.numTracks;
            t.pan = g.pan;
        }
    for (auto& g : slots)
    {
        if (! g.active)
            continue;
        if (g.numTracks == 0)
        {
            history[(size_t) historyPos] = { true, g.f0, g.pan, frame };
            historyPos = (historyPos + 1) % historySize;
            g = Group {};
            continue;
        }
        g.peakAmp = std::max (g.peakAmp, g.amp);
    }

    // Ungrouped tracks of age >= 2, strongest first.
    int count = 0;
    for (int i = 0; i < PartialTracker::maxTracks; ++i)
        if (tracks[(size_t) i].active && tracks[(size_t) i].group < 0 && tracks[(size_t) i].age >= 2)
            order[(size_t) count++] = i;
    std::sort (order.begin(), order.begin() + count, [&] (int a, int b)
    {
        return tracks[(size_t) a].amp != tracks[(size_t) b].amp ? tracks[(size_t) a].amp > tracks[(size_t) b].amp : a < b;
    });

    auto strongestIn = [&] (int gi)
    {
        double s = 0.0;
        for (const auto& t : tracks)
            if (t.active && t.group == gi)
                s = std::max (s, t.amp);
        return s;
    };
    auto join = [&] (PartialTracker::Track& t, int gi)
    {
        auto& g = slots[(size_t) gi];
        t.group = gi;
        t.pan = g.pan;
        g.amp += t.amp;
        g.peakAmp = std::max (g.peakAmp, g.amp);
        ++g.numTracks;
    };

    for (int o = 0; o < count; ++o)
    {
        auto& t = tracks[(size_t) order[(size_t) o]];
        int best = -1;
        double bestErr = 1e300;

        // Rule 1: harmonic of a group with a common onset. Rule 3: the same
        // without the onset condition, for weak late partials.
        auto harmonicSearch = [&] (bool needOnset)
        {
            best = -1;
            bestErr = 1e300;
            for (int gi = 0; gi < maxGroups; ++gi)
            {
                const auto& g = slots[(size_t) gi];
                if (! g.active)
                    continue;
                const double h = std::round (t.freq / g.f0);
                if (h < 1.0 || h > 20.0)
                    continue;
                const double err = std::abs (t.freq / (h * g.f0) - 1.0);
                if (err > harmonicTolerance)
                    continue;
                if (needOnset && std::abs (t.birthFrame - g.onsetFrame) > onsetWindow)
                    continue;
                if (! needOnset && t.amp > latePartialFactor * strongestIn (gi))
                    continue;
                if (err < bestErr)
                {
                    bestErr = err;
                    best = gi;
                }
            }
        };

        harmonicSearch (true);
        if (best >= 0)
        {
            join (t, best);
            continue;
        }

        // Rule 2: a new fundamental below a group with a common onset.
        for (int gi = 0; gi < maxGroups; ++gi)
        {
            const auto& g = slots[(size_t) gi];
            if (! g.active)
                continue;
            const double q = std::round (g.f0 / t.freq);
            if (q < 2.0 || q > 4.0)
                continue;
            const double err = std::abs (g.f0 / (q * t.freq) - 1.0);
            if (err <= harmonicTolerance && std::abs (t.birthFrame - g.onsetFrame) <= onsetWindow && err < bestErr)
            {
                bestErr = err;
                best = gi;
            }
        }
        if (best >= 0)
        {
            // A group founded by an upper partial a frame or two before its
            // fundamental was placed by the wrong pitch; while still inside
            // its onset window it is placed again by the fundamental.
            auto& g = slots[(size_t) best];
            g.f0 = t.freq;
            if (frame - g.onsetFrame <= onsetWindow)
                g.pan = assignPan (g.f0, frame, bassCentreHz, best);
            join (t, best);
            continue;
        }

        harmonicSearch (false);
        if (best >= 0)
        {
            join (t, best);
            continue;
        }

        // Rule 4: a new group, if a slot is free.
        if (numActive() < std::clamp (maxActiveGroups, 1, maxGroups))
        {
            for (int gi = 0; gi < maxGroups; ++gi)
            {
                auto& g = slots[(size_t) gi];
                if (g.active)
                    continue;
                const double pan = assignPan (t.freq, frame, bassCentreHz);
                g = Group {};
                g.active = true;
                g.f0 = t.freq;
                g.onsetFrame = t.birthFrame;
                g.pan = pan;
                join (t, gi);
                break;
            }
            continue;
        }

        // Rule 5: no slot.
        t.pan = 0.0;
    }
}
} // namespace sph
