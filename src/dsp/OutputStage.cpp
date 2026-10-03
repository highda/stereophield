#include "dsp/OutputStage.h"

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
    listen = p.listen;
    if (snap)
        reset();
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

        if (listen == Listen::Mono)
            l = r = 0.5f * (l + r);
        else if (listen == Listen::Side)
            l = r = 0.5f * (l - r);

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
