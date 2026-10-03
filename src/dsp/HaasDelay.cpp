#include "dsp/HaasDelay.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sph
{
void HaasDelay::prepare (const ProcessSpec& spec)
{
    fs = spec.sampleRate;
    delay.prepare ((int) std::ceil (0.040 * fs) + 2);
    fader.prepare (fs);
    timeSmoother.setTimeConstant (0.050, fs);
    lpHz.reset (fs, 0.020);
    reset();
}

void HaasDelay::reset()
{
    delay.reset();
    lpState = 0.0;
    timeSmoother.reset (timeTargetSamples);
    lpHz.setCurrentAndTargetValue (lpHz.getTargetValue());
}

void HaasDelay::setParams (const Params& p, bool snap)
{
    wanted = { p.delaySource, p.delaySide };
    timeTargetSamples = (float) (p.delayTimeMs * 0.001 * fs);
    lpHz.setTargetValue (p.delayLpHz);
    if (snap)
    {
        current = wanted;
        fader.snap();
        reset();
    }
    else if (! (wanted == current))
        fader.request();
}

void HaasDelay::process (const Buses& buses, float* s, float* m, int numSamples) noexcept
{
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (64, numSamples - done);
        lpHz.skip (n);
        const double fc = lpHz.getCurrentValue();
        // One-pole low-pass, bypassed at its 20 kHz maximum.
        const bool lpOn = fc < 20000.0;
        const double a = std::exp (-2.0 * std::numbers::pi * std::min (fc, 0.49 * fs) / fs);

        for (int i = done; i < done + n; ++i)
        {
            if (fader.readyToApply())
            {
                current = wanted;
                delay.reset();
                lpState = 0.0;
                fader.applied();
            }
            const float g = fader.next();
            const float u = buses.get (current.source)[i];
            delay.push (u);
            const double t = timeSmoother.process (timeTargetSamples);
            double d = delay.readLinear (t);
            if (lpOn)
            {
                lpState = a * lpState + (1.0 - a) * d;
                d = lpState;
            }
            else
                lpState = d;
            const float q = current.side == HaasSide::Right ? 1.0f : -1.0f;
            s[i] = g * q * (u - (float) d) * 0.5f;
            m[i] = g * ((float) d - u) * 0.5f;
        }
        done += n;
    }
}

int HaasDelay::tailSamples() const noexcept
{
    const double t = std::max ((double) timeTargetSamples, timeSmoother.current());
    return (int) std::ceil (t + 0.005 * fs);
}
} // namespace sph
