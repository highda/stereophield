#include "dsp/DoubleTracker.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sph
{
void DoubleTracker::Drift::setRate (double hz, double rate) noexcept
{
    a = std::exp (-2.0 * std::numbers::pi * hz / rate);
    // Variance of white noise through ((1 - a) / (1 - a z^-1))^2.
    const double var = std::pow (1.0 - a, 4.0) * (1.0 + a * a) / std::pow (1.0 - a * a, 3.0);
    norm = 1.0 / std::sqrt (std::max (var, 1e-30));
}

void DoubleTracker::prepare (const ProcessSpec& spec)
{
    fs = spec.sampleRate;
    controlRate = fs / controlInterval;
    line.prepare ((int) std::ceil (0.062 * fs) + 4);
    fader.prepare (fs);
    offsetMs.setTimeConstant (0.050, fs / controlInterval);
    for (auto& tk : takes)
        tk.delaySmooth.setTimeConstant (0.003, fs);
    appliedRate = -1.0f;
    appliedTone = 1e9f;
    updateRates();
    seedTakes();
    reset();
}

void DoubleTracker::seedTakes() noexcept
{
    for (int t = 0; t < 2; ++t)
    {
        auto& tk = takes[(size_t) t];
        tk.rng.setSeed (3000u + 2u * (uint32_t) current.seed + (uint32_t) t);
        tk.timing.y1 = tk.timing.y2 = tk.wow.y1 = tk.wow.y2 = tk.level.y1 = tk.level.y2 = 0.0;
    }
    phase = 0;
}

void DoubleTracker::updateRates() noexcept
{
    if (driftRate != appliedRate)
    {
        appliedRate = driftRate;
        const double wowRate = std::max (1.5, 4.0 * driftRate);
        for (auto& tk : takes)
        {
            tk.timing.setRate (driftRate, controlRate);
            tk.wow.setRate (wowRate, controlRate);
            tk.level.setRate (0.5 * driftRate, controlRate);
        }
        // Standard deviation of the wow's time derivative per unit of its own
        // standard deviation, measured on a 60 s run (the cascaded one-pole
        // has no simple closed form for it).
        Drift d;
        d.setRate (wowRate, controlRate);
        Rng rng (12345u);
        double prev = d.step (rng), sum2 = 0.0;
        const int steps = (int) (60.0 * controlRate);
        for (int i = 0; i < steps; ++i)
        {
            const double v = d.step (rng);
            sum2 += (v - prev) * (v - prev);
            prev = v;
        }
        wowSlope = std::sqrt (sum2 / steps) * controlRate; // per second
    }
    // Pitch deviation (ratio - 1) equals minus the delay's slope.
    wowAmplitudeSeconds = (std::pow (2.0, pitchCents / 1200.0) - 1.0) / wowSlope;
    if (toneDb != appliedTone)
    {
        appliedTone = toneDb;
        for (int t = 0; t < 2; ++t)
        {
            const float g = juce::Decibels::decibelsToGain ((t == 0 ? 0.5f : -0.5f) * toneDb);
            takes[(size_t) t].shelf.setCoefficients (
                juce::dsp::IIR::ArrayCoefficients<double>::makeHighShelf (fs, std::min (4000.0, 0.4 * fs), 0.7071, g));
        }
    }
}

void DoubleTracker::reset()
{
    line.reset();
    for (auto& tk : takes)
        tk.shelf.reset();
    offsetMs.reset (offsetTarget);
    // Interpolation targets start from the current drift state so no jump.
    for (int t = 0; t < 2; ++t)
    {
        auto& tk = takes[(size_t) t];
        const double d = std::clamp (offsetTarget * 0.001 + driftMs * 0.001 * tk.timing.y2 * tk.timing.norm
                                         + wowAmplitudeSeconds * tk.wow.y2 * tk.wow.norm, 0.001, 0.060) * fs;
        tk.prevDelay = tk.nextDelay = d;
        tk.delaySmooth.reset (d);
        tk.prevGain = tk.nextGain = std::pow (10.0, levelDb * tk.level.y2 * tk.level.norm / 20.0);
        lastDelay[(size_t) t] = d;
    }
}

void DoubleTracker::setParams (const Params& p, bool snap)
{
    wanted = { p.dblSource, std::clamp (p.dblSeed, 0, 15) };
    offsetTarget = p.dblOffsetMs;
    driftMs = p.dblDriftMs;
    driftRate = p.dblDriftRate;
    pitchCents = p.dblPitchCents;
    levelDb = p.dblLevelDb;
    toneDb = p.dblToneDb;
    updateRates();
    if (snap)
    {
        current = wanted;
        fader.snap();
        seedTakes();
        reset();
    }
    else if (! (wanted == current) && ! externalSwitching)
        fader.request();
}

void DoubleTracker::controlStep() noexcept
{
    const double off = offsetMs.process (offsetTarget) * 0.001;
    for (auto& tk : takes)
    {
        tk.prevDelay = tk.nextDelay;
        tk.prevGain = tk.nextGain;
        const double t = off + driftMs * 0.001 * tk.timing.step (tk.rng);
        const double w = wowAmplitudeSeconds * tk.wow.step (tk.rng);
        tk.nextDelay = std::clamp (t + w, 0.001, 0.060) * fs;
        tk.nextGain = std::pow (10.0, levelDb * tk.level.step (tk.rng) / 20.0);
    }
}

void DoubleTracker::copyDriftFrom (const DoubleTracker& o) noexcept
{
    for (int t = 0; t < 2; ++t)
    {
        auto& a = takes[(size_t) t];
        const auto& b = o.takes[(size_t) t];
        a.rng = b.rng;
        a.timing.y1 = b.timing.y1;
        a.timing.y2 = b.timing.y2;
        a.wow.y1 = b.wow.y1;
        a.wow.y2 = b.wow.y2;
        a.level.y1 = b.level.y1;
        a.level.y2 = b.level.y2;
        a.prevDelay = b.prevDelay;
        a.nextDelay = b.nextDelay;
        a.prevGain = b.prevGain;
        a.nextGain = b.nextGain;
        a.delaySmooth.reset (b.delaySmooth.current());
    }
    phase = o.phase;
    offsetMs.reset (o.offsetMs.current());
}

void DoubleTracker::advanceWhileAsleep (int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
        if (++phase >= controlInterval)
        {
            phase = 0;
            controlStep();
        }
}

void DoubleTracker::process (const Buses& buses, float* s, float* m, int numSamples) noexcept
{
    constexpr double inv = 1.0 / controlInterval;
    for (int i = 0; i < numSamples; ++i)
    {
        if (fader.readyToApply())
        {
            const bool reseed = wanted.seed != current.seed;
            current = wanted;
            line.reset();
            if (reseed)
                seedTakes();
            reset();
            fader.applied();
        }
        const float g = fader.next();
        const float u = buses.get (current.source)[i];
        line.push (u);
        if (++phase >= controlInterval)
        {
            phase = 0;
            controlStep();
        }
        const double t = phase * inv;
        float out[2];
        for (int k = 0; k < 2; ++k)
        {
            auto& tk = takes[(size_t) k];
            const double d = tk.delaySmooth.process (tk.prevDelay + t * (tk.nextDelay - tk.prevDelay));
            const double gain = tk.prevGain + t * (tk.nextGain - tk.prevGain);
            lastDelay[(size_t) k] = d;
            out[k] = (float) (gain * tk.shelf.process (line.readLagrange (d)));
        }
        if (tapA != nullptr)
        {
            tapA[i] = out[0];
            tapB[i] = out[1];
        }
        s[i] = g * 0.5f * (out[0] - out[1]);
        m[i] = g * (0.5f * (out[0] + out[1]) - u);
    }
}
} // namespace sph
