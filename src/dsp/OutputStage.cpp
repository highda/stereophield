#include "dsp/OutputStage.h"

#include <algorithm>
#include <cmath>

namespace sph
{
namespace
{
inline float flushTiny (float x) noexcept { return std::abs (x) < 1.0e-30f ? 0.0f : x; }
} // namespace

void OutputStage::prepare (const ProcessSpec& spec)
{
    fs = spec.sampleRate;
    for (auto* sv : { &gain, &compMix, &bypassMix })
        sv->reset (fs, 0.020);
    pm2.setTimeConstant (0.300, fs);
    listenFadeLen = std::max (1, (int) std::lround (0.020 * fs));
    ps2.setTimeConstant (0.300, fs);
    reset();
}

void OutputStage::reset()
{
    for (auto* sv : { &gain, &compMix, &bypassMix })
        sv->setCurrentAndTargetValue (sv->getTargetValue());
    pm2.reset();
    ps2.reset();
    lastC = 1.0f;
}

void OutputStage::setParams (const Params& p, bool snap)
{
    gain.setTargetValue (std::pow (10.0f, p.outGainDb / 20.0f));
    compMix.setTargetValue (p.compMode == CompMode::ConstantLoudness ? 1.0f : 0.0f);
    bypassMix.setTargetValue (p.bypass ? 1.0f : 0.0f);
    if (p.listen != listen)
    {
        // Crossfade from the mode currently heard (PART2_LEDGER.md I6).
        listenFrom = listenFade > 0 ? listenFrom : listen;
        listen = p.listen;
        listenFade = snap ? 0 : listenFadeLen;
    }
    if (snap)
    {
        listenFade = 0;
        reset();
    }
}

void OutputStage::process (const float* mOut, const float* sSyn, const float* mD, const float* sInD,
                           float* outL, float* outR, int numSamples) noexcept
{
    const bool compRun = compMix.isSmoothing() || compMix.getTargetValue() > 0.0f;
    const bool bypassRun = bypassMix.isSmoothing();
    const bool bypassed = ! bypassRun && bypassMix.getTargetValue() > 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float sOut = sSyn[i] + sInD[i];
        float l = mOut[i] + sOut;
        float r = mOut[i] - sOut;

        const float wC = compMix.getNextValue();
        if (compRun)
        {
            const double a = pm2.process ((double) mOut[i] * mOut[i]);
            const double b = ps2.process ((double) sOut * sOut);
            const float c = a + b < 1.0e-12 ? 1.0f : (float) std::sqrt (a / (a + b));
            const float cEff = (1.0f - wC) + wC * c;
            lastC = cEff;
            l *= cEff;
            r *= cEff;
        }

        const float g = gain.getNextValue();
        l *= g;
        r *= g;

        auto apply = [] (Listen mode, float& a, float& b)
        {
            if (mode == Listen::Mono)
                a = b = 0.5f * (a + b);
            else if (mode == Listen::Side)
                a = b = 0.5f * (a - b);
        };
        if (listenFade > 0)
        {
            float l0 = l, r0 = r;
            apply (listenFrom, l0, r0);
            apply (listen, l, r);
            const float w = (float) listenFade / (float) listenFadeLen;
            l = w * l0 + (1.0f - w) * l;
            r = w * r0 + (1.0f - w) * r;
            --listenFade;
        }
        else
            apply (listen, l, r);

        const float wB = bypassMix.getNextValue();
        const float bl = mD[i] + sInD[i], br = mD[i] - sInD[i];
        if (bypassed)
        {
            l = bl;
            r = br;
        }
        else if (bypassRun)
        {
            l = (1.0f - wB) * l + wB * bl;
            r = (1.0f - wB) * r + wB * br;
        }

        outL[i] = flushTiny (l);
        outR[i] = flushTiny (r);
    }

    if (! compRun)
    {
        pm2.reset();
        ps2.reset();
    }
}
} // namespace sph
