#include "dsp/Velvet.h"

#include "dsp/Rng.h"
#include "dsp/VelvetTables.h"

#include <algorithm>
#include <cmath>

namespace sph
{
void Velvet::build (Sequence& seq, double sampleRate, float sizeMs, float density, uint32_t seed) noexcept
{
    const int len = std::max (1, (int) std::lround (sizeMs * 0.001 * sampleRate));
    const int count = std::clamp ((int) std::lround (density * sizeMs * 0.001), 8, maxImpulses);
    const double td = (double) len / count;
    Rng rng (seed);
    double energy = 0.0;
    for (int j = 0; j < count; ++j)
    {
        const double r1 = rng.uniform();
        const double r2 = rng.uniform();
        seq.pos[(size_t) j] = std::clamp ((int) std::lround (j * td + r1 * (td - 1.0)), 0, len - 1);
        const double g = (r2 < 0.5 ? -1.0 : 1.0) * std::exp (-6.908 * j / count);
        seq.gain[(size_t) j] = (float) g;
        energy += g * g;
    }
    const float norm = (float) (1.0 / std::sqrt (energy));
    for (int j = 0; j < count; ++j)
        seq.gain[(size_t) j] *= norm;
    seq.count = count;
}

void Velvet::buildOptimised (Sequence& seq, double sampleRate, float sizeMs, float density, int variation, int side) noexcept
{
    using namespace velvet_tables;
    const int len = std::max (1, (int) std::lround (sizeMs * 0.001 * sampleRate));
    const int wanted = std::clamp ((int) std::lround (density * sizeMs * 0.001), 8, maxImpulses);
    int cls = 0, offset = 0, at = 0;
    for (int c = 0; c < numClasses; ++c)
    {
        if (std::abs (classCounts[c] - wanted) < std::abs (classCounts[cls] - wanted))
        {
            cls = c;
            offset = at;
        }
        at += classCounts[c];
    }
    const int count = classCounts[cls];
    const uint16_t* d = data[std::clamp (variation, 0, 15)][side] + offset;
    double energy = 0.0;
    for (int j = 0; j < count; ++j)
    {
        const double frac = (d[j] >> 1) / 32768.0;
        seq.pos[(size_t) j] = std::clamp ((int) std::lround (frac * len), 0, len - 1);
        const double g = ((d[j] & 1) ? -1.0 : 1.0) * std::exp (-6.908 * j / count);
        seq.gain[(size_t) j] = (float) g;
        energy += g * g;
    }
    const float norm = (float) (1.0 / std::sqrt (energy));
    for (int j = 0; j < count; ++j)
        seq.gain[(size_t) j] *= norm;
    seq.count = count;
}

void Velvet::prepare (const ProcessSpec& spec)
{
    fs = spec.sampleRate;
    line.prepare ((int) std::ceil (0.080 * fs) + 2);
    fader.prepare (fs);
    reset();
}

void Velvet::reset()
{
    line.reset();
}

void Velvet::rebuild() noexcept
{
    if (current.design == VelvetDesign::Optimised)
    {
        buildOptimised (seqL, fs, current.sizeMs, current.density, current.variation, 0);
        buildOptimised (seqR, fs, current.sizeMs, current.density, current.variation, 1);
    }
    else
    {
        build (seqL, fs, current.sizeMs, current.density, 1000u + 2u * (uint32_t) current.variation);
        build (seqR, fs, current.sizeMs, current.density, 1001u + 2u * (uint32_t) current.variation);
    }
    length = std::max (1, (int) std::lround (current.sizeMs * 0.001 * fs));
}

void Velvet::setParams (const Params& p, bool snap)
{
    wanted = { p.velvetSource, p.velvetSizeMs, p.velvetDensity, std::clamp (p.velvetVariation, 0, 15), p.velvetDesign };
    if (snap)
    {
        current = wanted;
        fader.snap();
        rebuild();
        reset();
    }
    else if (! (wanted == current) && ! externalSwitching)
        fader.request();
}

void Velvet::process (const Buses& buses, float* s, float* m, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        if (fader.readyToApply())
        {
            current = wanted;
            rebuild();
            line.reset();
            fader.applied();
        }
        const float g = fader.next();
        const float u = buses.get (current.source)[i];
        line.push (u);
        float vl = 0.0f, vr = 0.0f;
        for (int j = 0; j < seqL.count; ++j)
            vl += seqL.gain[(size_t) j] * line.readInt (seqL.pos[(size_t) j]);
        for (int j = 0; j < seqR.count; ++j)
            vr += seqR.gain[(size_t) j] * line.readInt (seqR.pos[(size_t) j]);
        if (tapLeft != nullptr)
        {
            tapLeft[i] = vl;
            tapRight[i] = vr;
        }
        s[i] = g * 0.5f * (vl - vr);
        m[i] = g * (0.5f * (vl + vr) - u);
    }
}
} // namespace sph
