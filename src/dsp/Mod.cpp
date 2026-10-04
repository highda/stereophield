#include "dsp/Mod.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sph
{
namespace
{
inline double wrap (double x) noexcept { return x - std::floor (x); }
} // namespace

void Mod::prepare (const ProcessSpec& spec)
{
    fs = spec.sampleRate;
    window = 0.040 * fs;
    // Longest read: the pitch window plus the right pre-delay (30 + 8 ms).
    line.prepare ((int) std::ceil (window + 0.038 * fs) + 4);
    fader.prepare (fs);
    rateHz.reset (fs, 0.020);
    cents.reset (fs, 0.020);
    for (auto* p : { &baseMs, &depthMs, &predelayMs })
        p->setTimeConstant (0.050, fs);

    // Phases start at zero only here.
    phase = 0.0;
    phiL = phiR = 0.0;
    reset();
    lfoSmooth.reset (4.0 * std::abs (phase - 0.5) - 1.0);
}

void Mod::reset()
{
    line.reset();
    rateHz.setCurrentAndTargetValue (rateHz.getTargetValue());
    cents.setCurrentAndTargetValue (cents.getTargetValue());
    baseMs.reset (baseTarget);
    depthMs.reset (depthTarget);
    predelayMs.reset (predelayTarget);
    updateRates();
}

void Mod::setParams (const Params& p, bool snap)
{
    wanted = { p.modSource, p.modType };
    rateHz.setTargetValue (p.modRateHz);
    cents.setTargetValue (p.modCents);
    baseTarget = p.modBaseMs;
    depthTarget = p.modDepthMs;
    predelayTarget = p.modPredelayMs;
    if (snap)
    {
        current = wanted;
        fader.snap();
        reset();
    }
    else if (! (wanted == current) && ! externalSwitching)
        fader.request();
}

void Mod::updateRates() noexcept
{
    const double rate = std::max (1.0e-3, (double) rateHz.getCurrentValue());
    phaseInc = rate / fs;
    lfoSmooth.setTimeConstant (0.05 / rate, fs);
    // ratio = 2^(cents / 1200) with +cents on the left and -cents on the right.
    const double c = cents.getCurrentValue();
    phiIncL = std::abs (std::pow (2.0, c / 1200.0) - 1.0) / window;
    phiIncR = std::abs (std::pow (2.0, -c / 1200.0) - 1.0) / window;
}

void Mod::stepLfo() noexcept
{
    phase = wrap (phase + phaseInc);
    lfoSmooth.process (4.0 * std::abs (phase - 0.5) - 1.0);
}

void Mod::stepPhasors() noexcept
{
    // With zero cents the phasors hold still.
    phiL = wrap (phiL + phiIncL);
    phiR = wrap (phiR + phiIncR);
}

void Mod::copyPhaseFrom (const Mod& o) noexcept
{
    phase = o.phase;
    lfoSmooth.reset (o.lfoSmooth.current());
    phiL = o.phiL;
    phiR = o.phiR;
}

void Mod::advanceWhileAsleep (int numSamples) noexcept
{
    // The LFO and its one-pole run per sample so the smoothed value stays
    // current.
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (64, numSamples - done);
        const bool smoothing = rateHz.isSmoothing() || cents.isSmoothing();
        rateHz.skip (n);
        cents.skip (n);
        if (smoothing)
            updateRates();
        for (int i = 0; i < n; ++i)
        {
            stepLfo();
            stepPhasors();
        }
        done += n;
    }
}

void Mod::process (const Buses& buses, float* s, float* m, int numSamples) noexcept
{
    const double msToSamples = 0.001 * fs;
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (64, numSamples - done);
        const bool smoothing = rateHz.isSmoothing() || cents.isSmoothing();
        rateHz.skip (n);
        cents.skip (n);
        if (smoothing)
            updateRates();

        for (int i = done; i < done + n; ++i)
        {
            if (fader.readyToApply())
            {
                current = wanted;
                line.reset();
                fader.applied();
            }
            const float g = fader.next();
            const float u = buses.get (current.source)[i];
            line.push (u);

            stepLfo();
            stepPhasors();
            const double base = baseMs.process (baseTarget);
            const double depth = depthMs.process (depthTarget);
            const double pre = predelayMs.process (predelayTarget);

            float pL, pR;
            if (current.type == ModType::Chorus)
            {
                const double l = lfoSmooth.current();
                lastDL = std::max (0.5, base + depth * l);
                lastDR = std::max (0.5, base - depth * l);
                pL = line.readLagrange (lastDL * msToSamples);
                pR = line.readLagrange (lastDR * msToSamples);
            }
            else
            {
                auto shifter = [&] (double phi, bool up, double preMs)
                {
                    const double phi2 = wrap (phi + 0.5);
                    const double d1 = (up ? 1.0 - phi : phi) * window;
                    const double d2 = (up ? 1.0 - phi2 : phi2) * window;
                    const double s1 = std::sin (std::numbers::pi * phi), s2 = std::sin (std::numbers::pi * phi2);
                    const double preSamples = preMs * msToSamples;
                    return (float) (s1 * s1 * line.readLagrange (d1 + preSamples)
                                    + s2 * s2 * line.readLagrange (d2 + preSamples));
                };
                pL = shifter (phiL, true, pre);
                pR = shifter (phiR, false, pre + 8.0);
            }

            if (tapLeft != nullptr)
            {
                tapLeft[i] = pL;
                tapRight[i] = pR;
            }
            s[i] = g * 0.5f * (pL - pR);
            m[i] = g * 0.5f * (0.5f * (pL + pR) - u);
        }
        done += n;
    }
}
} // namespace sph
