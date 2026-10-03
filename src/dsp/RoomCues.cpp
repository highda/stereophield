#include "dsp/RoomCues.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sph
{
void RoomCues::build (TapSet& set, const Geometry& g, double sampleRate) noexcept
{
    const double c = 343.0;
    const double lx = g.size, ly = 0.8 * g.size, lz = 3.0;
    const double listener[3] = { lx / 2, ly / 2, 1.2 };
    const double dist = std::clamp ((double) g.distance, 0.3, ly / 2 - 0.1);
    const double src[3] = { lx / 2, ly / 2 + dist, 1.2 };
    const double len[3] = { lx, ly, lz };
    const double r0 = dist;
    const double beta = std::sqrt (std::max (0.0, 1.0 - (double) g.absorb));

    set.count = 0;
    const int order = std::clamp (g.order, 1, 2);
    for (int ix = -order; ix <= order; ++ix)
        for (int iy = -order; iy <= order; ++iy)
            for (int iz = -order; iz <= order; ++iz)
            {
                const int hits = std::abs (ix) + std::abs (iy) + std::abs (iz);
                if (hits == 0 || hits > order || set.count >= maxTaps)
                    continue;
                const int idx[3] = { ix, iy, iz };
                double v[3];
                for (int a = 0; a < 3; ++a)
                {
                    // Image coordinate on one axis after |i| reflections.
                    const int i = idx[a];
                    const double pos = (i % 2 == 0) ? i * len[a] + src[a] : (i + 1) * len[a] - src[a];
                    v[a] = pos - listener[a];
                }
                const double r = std::sqrt (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                const double tau = (r - r0) / c;
                if (tau <= 0.0 || tau > maxDelaySeconds)
                    continue;
                // Azimuth in the horizontal plane, positive to the left (x
                // grows to the right, y to the front).
                const double theta = std::atan2 (-v[0], v[1]);
                const double gain = (r0 / r) * std::pow (beta, hits);
                auto& t = set.taps[(size_t) set.count++];
                t.delay = (int) std::lround (tau * sampleRate);
                t.side = (float) (gain * std::sin (theta));
                t.mid = (float) (gain * std::abs (std::cos (theta)));
                const double fc = std::min ((double) g.damp / std::sqrt ((double) hits), 0.45 * sampleRate);
                t.lpCoeff = (float) std::exp (-2.0 * std::numbers::pi * fc / sampleRate);
                t.hits = hits;
                t.azimuthDeg = (float) (theta * 180.0 / std::numbers::pi);
            }
}

void RoomCues::prepare (const ProcessSpec& spec)
{
    fs = spec.sampleRate;
    line.prepare ((int) std::ceil (maxDelaySeconds * fs) + 2);
    fadeLen = std::max (1, (int) std::lround (0.030 * fs));
    build (sets[0], current, fs);
    activeSet = 0;
    fadePos = fadeLen;
    reset();
}

void RoomCues::reset()
{
    line.reset();
    for (auto& s : lpState)
        s.fill (0.0f);
}

void RoomCues::setParams (const Params& p, bool snap)
{
    source = p.roomSource;
    wanted = { p.roomSize, p.roomDistance, p.roomAbsorb, p.roomDampHz, p.roomOrder };
    if (snap)
    {
        current = wanted;
        build (sets[(size_t) activeSet], current, fs);
        fadePos = fadeLen;
        reset();
    }
}

void RoomCues::process (const Buses& buses, float* s, float* m, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        // Start a crossfade to the wanted geometry when none is running.
        if (fadePos >= fadeLen && ! (wanted == current))
        {
            current = wanted;
            const int next = 1 - activeSet;
            build (sets[(size_t) next], current, fs);
            lpState[(size_t) next].fill (0.0f);
            activeSet = next;
            fadePos = 0;
        }
        line.push (buses.get (source)[i]);

        float so[2] = { 0, 0 }, mo[2] = { 0, 0 };
        const int setsToRun = fadePos < fadeLen ? 2 : 1;
        for (int k = 0; k < setsToRun; ++k)
        {
            const int which = k == 0 ? activeSet : 1 - activeSet;
            const auto& set = sets[(size_t) which];
            auto& st = lpState[(size_t) which];
            for (int t = 0; t < set.count; ++t)
            {
                const auto& tap = set.taps[(size_t) t];
                float& y = st[(size_t) t];
                y = tap.lpCoeff * y + (1.0f - tap.lpCoeff) * line.readInt (tap.delay);
                so[k] += tap.side * y;
                mo[k] += tap.mid * y;
            }
        }
        if (setsToRun == 2)
        {
            const float x = (float) fadePos / (float) fadeLen;
            const float gNew = std::sin (0.5f * std::numbers::pi_v<float> * x);
            const float gOld = std::cos (0.5f * std::numbers::pi_v<float> * x);
            s[i] = gNew * so[0] + gOld * so[1];
            m[i] = gNew * mo[0] + gOld * mo[1];
            ++fadePos;
        }
        else
        {
            s[i] = so[0];
            m[i] = mo[0];
        }
    }
}
} // namespace sph
