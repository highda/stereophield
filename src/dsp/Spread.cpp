#include "dsp/Spread.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sph
{
void Spread::prepare (const ProcessSpec& spec)
{
    fs = spec.sampleRate;
    delay.prepare ((int) std::ceil (0.030 * fs) + 1);
    fader.prepare (fs);
    timeSmoother.setTimeConstant (0.050, fs);
    for (auto* sv : { &fLo, &fHi, &skew, &q })
        sv->reset (fs, 0.020);
    reset();
}

void Spread::reset()
{
    resetState();
    timeSmoother.reset (timeTargetSamples);
    for (auto* sv : { &fLo, &fHi, &skew, &q })
        sv->setCurrentAndTargetValue (sv->getTargetValue());
    coeffsDirty = true;
}

void Spread::resetState() noexcept
{
    delay.reset();
    for (auto& b : sections)
        b.reset();
}

void Spread::setParams (const Params& p, bool snap)
{
    wanted = { p.spreadSource, p.spreadType, std::clamp (p.spreadDensity, 2, maxSections) };
    timeTargetSamples = (float) (p.spreadTimeMs * 0.001 * fs);
    fLo.setTargetValue (p.spreadFLo);
    fHi.setTargetValue (p.spreadFHi);
    skew.setTargetValue (p.spreadSkew);
    q.setTargetValue (p.spreadQ);

    if (snap)
    {
        current = wanted;
        fader.snap();
        reset();
    }
    else if (! (wanted == current) && ! externalSwitching)
        fader.request();
}

void Spread::updateCoefficients() noexcept
{
    const int k = current.density;
    const double lo = fLo.getCurrentValue(), hi = fHi.getCurrentValue();
    const double gamma = std::pow (2.0, (double) skew.getCurrentValue());
    const double qq = q.getCurrentValue();
    for (int i = 0; i < k; ++i)
    {
        const double x = (i + 0.5) / k;
        const double f = std::min (lo * std::pow (hi / lo, std::pow (x, gamma)), 0.45 * fs);
        freqs[(size_t) i] = f;
        sections[(size_t) i].setCoefficients (Biquad::allPass (fs, f, qq));
        const double alpha = std::sin (2.0 * std::numbers::pi * f / fs) / (2.0 * qq);
        radii[(size_t) i] = std::sqrt ((1.0 - alpha) / (1.0 + alpha));
    }
    coeffsDirty = false;
}

void Spread::process (const Buses& buses, float* s, int numSamples) noexcept
{
    int done = 0;
    while (done < numSamples)
    {
        // Sub-blocks of at most 64 samples; coefficients follow the smoothed
        // frequencies once per sub-block.
        const int n = std::min (64, numSamples - done);
        const bool smoothing = fLo.isSmoothing() || fHi.isSmoothing() || skew.isSmoothing() || q.isSmoothing();
        for (auto* sv : { &fLo, &fHi, &skew, &q })
            sv->skip (n);
        if ((smoothing || coeffsDirty) && current.type == SpreadType::Cascade)
            updateCoefficients();

        for (int i = done; i < done + n; ++i)
        {
            if (fader.readyToApply())
            {
                current = wanted;
                resetState();
                coeffsDirty = true;
                if (current.type == SpreadType::Cascade)
                    updateCoefficients();
                fader.applied();
            }
            const float g = fader.next();
            const float x = buses.get (current.source)[i];
            const double t = timeSmoother.process (timeTargetSamples);

            float y;
            if (current.type == SpreadType::Delay)
            {
                delay.push (x);
                y = delay.readInt ((int) std::lround (t));
            }
            else
            {
                double v = x;
                for (int k = 0; k < current.density; ++k)
                    v = sections[(size_t) k].process (v);
                y = (float) v;
            }
            s[i] = g * y;
        }
        done += n;
    }
}

int Spread::tailSamples() const noexcept
{
    if (current.type == SpreadType::Delay)
        return (int) std::ceil (std::max ((double) timeTargetSamples, timeSmoother.current())) + 1;

    // 100 ms, or longer when a narrow low section rings longer: 1.5 times the
    // time the slowest section takes to decay by 100 dB.
    double longest = 0.0;
    for (int k = 0; k < current.density; ++k)
        if (radii[(size_t) k] > 0.0 && radii[(size_t) k] < 1.0)
            longest = std::max (longest, std::log (1.0e-5) / std::log (radii[(size_t) k]));
    return (int) std::ceil (std::max (0.100 * fs, 1.5 * longest));
}
} // namespace sph
