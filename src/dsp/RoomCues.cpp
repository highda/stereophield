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
    // The array sits off the room's centre lines: in a mirror-symmetric
    // arrangement each reflection's mirror image delivers to one microphone
    // exactly what the original delivers to the other, and L = R.
    const double centre[3] = { asymX * lx, asymY * ly, 1.2 };
    const double dist = std::clamp ((double) g.distance, 0.3, (1.0 - asymY) * ly - 0.1);
    const double src[3] = { centre[0], centre[1] + dist, 1.2 };
    const double len[3] = { lx, ly, lz };
    const double beta = std::sqrt (std::max (0.0, 1.0 - (double) g.absorb));
    // Microphones on the x axis; x grows to the right, y to the front.
    const double micX[2] = { -micSpacing / 2, micSpacing / 2 };          // left, right
    const double micAxis[2] = { micAngleDeg * std::numbers::pi / 180.0, // left mic points left
                                -micAngleDeg * std::numbers::pi / 180.0 };
    // Direct-path distance to each microphone; reflections are delayed
    // relative to the direct sound at the array centre.
    const double r0 = dist;

    set.count = 0;
    const int order = std::clamp (g.order, 1, 2);
    for (int ix = -order; ix <= order; ++ix)
        for (int iy = -order; iy <= order; ++iy)
            for (int iz = -order; iz <= order; ++iz)
            {
                const int hits = std::abs (ix) + std::abs (iy) + std::abs (iz);
                if (hits == 0 || hits > order || set.count >= maxReflections)
                    continue;
                const int idx[3] = { ix, iy, iz };
                double img[3];
                for (int a = 0; a < 3; ++a)
                {
                    // Image coordinate on one axis after |i| reflections.
                    const int i = idx[a];
                    img[a] = (i % 2 == 0) ? i * len[a] + src[a] : (i + 1) * len[a] - src[a];
                }
                const double vx = img[0] - centre[0], vy = img[1] - centre[1], vz = img[2] - centre[2];
                const double rc = std::sqrt (vx * vx + vy * vy + vz * vz);
                if ((rc - r0) / c <= 0.0 || (rc - r0) / c > maxDelaySeconds)
                    continue;
                auto& t = set.taps[(size_t) set.count++];
                // Azimuth, positive to the left.
                const double theta = std::atan2 (-vx, vy);
                for (int mic = 0; mic < 2; ++mic)
                {
                    const double dx = img[0] - (centre[0] + micX[mic]);
                    const double r = std::sqrt (dx * dx + vy * vy + vz * vz);
                    const double cosOff = std::cos (theta - micAxis[mic]) * std::sqrt (vx * vx + vy * vy) / rc;
                    const double cardioid = 0.5 + 0.5 * cosOff;
                    t.delay[mic] = (int) std::lround ((r - r0) / c * sampleRate);
                    t.gain[mic] = (float) ((r0 / r) * std::pow (beta, hits) * cardioid);
                }
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
    for (auto& st : lpState)
        for (auto& v : st)
            v = { 0.0f, 0.0f };
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
            for (auto& v : lpState[(size_t) next])
                v = { 0.0f, 0.0f };
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
            float mic[2] = { 0.0f, 0.0f };
            for (int t = 0; t < set.count; ++t)
            {
                const auto& tap = set.taps[(size_t) t];
                for (int c = 0; c < 2; ++c)
                {
                    float& y = st[(size_t) t][(size_t) c];
                    y = tap.lpCoeff * y + (1.0f - tap.lpCoeff) * line.readInt (tap.delay[c]);
                    mic[c] += tap.gain[c] * y;
                }
            }
            so[k] = 0.5f * (mic[0] - mic[1]);
            mo[k] = 0.5f * (mic[0] + mic[1]);
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
