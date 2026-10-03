#include "dsp/SideBus.h"

#include "dsp/Params.h"

#include <algorithm>
#include <cmath>

namespace sph
{
namespace
{
constexpr double butterworthQ = 0.70710678118654752440;
constexpr float bassMonoOff = 20.0f;

inline float flushTiny (float x) noexcept { return std::abs (x) < 1.0e-30f ? 0.0f : x; }
} // namespace

void SideBus::prepare (const ProcessSpec& spec)
{
    fs = spec.sampleRate;
    for (auto* sv : { &hpHz, &hpMix, &xLo, &xHi, &bandLow, &bandMid, &bandHigh, &splitMix,
                      &duck, &width, &midBlend, &guardMix })
        sv->reset (fs, 0.020);

    const juce::dsp::ProcessSpec js { fs, (juce::uint32) std::max (1, spec.maxBlockSize), 2 };
    lr1.prepare (js);
    lr2.prepare (js);
    ap2.prepare (js);
    ap2.setType (juce::dsp::LinkwitzRileyFilterType::allpass);

    guardPm.setTimeConstant (0.200, fs);
    guardPs.setTimeConstant (0.200, fs);
    guardG.setTimes (0.100, 0.005, fs); // rises in 100 ms, falls in 5 ms
    reset();
}

void SideBus::reset()
{
    for (auto* sv : { &hpHz, &hpMix, &xLo, &xHi, &bandLow, &bandMid, &bandHigh, &splitMix,
                      &duck, &width, &midBlend, &guardMix })
        sv->setCurrentAndTargetValue (sv->getTargetValue());
    for (auto& bus : hp)
        for (auto& b : bus)
            b.reset();
    lr1.reset();
    lr2.reset();
    ap2.reset();
    guardPm.reset();
    guardPs.reset();
    guardG.reset (1.0);
    updateFilters();
}

void SideBus::setParams (const Params& p, bool snap)
{
    hpHz.setTargetValue (std::max (p.bassMonoHz, bassMonoOff));
    hpMix.setTargetValue (p.bassMonoHz > bassMonoOff ? 1.0f : 0.0f);
    xLo.setTargetValue (p.bandXoverLo);
    xHi.setTargetValue (p.bandXoverHi);
    bandLow.setTargetValue (p.bandLow);
    bandMid.setTargetValue (p.bandMid);
    bandHigh.setTargetValue (p.bandHigh);
    duck.setTargetValue (p.transientDuck);
    width.setTargetValue (p.width);
    midBlend.setTargetValue (p.midBlend);
    guardMix.setTargetValue (p.guard ? 1.0f : 0.0f);

    // The split runs (crossfaded in) unless the three gains are equal and settled.
    const bool equal = p.bandLow == p.bandMid && p.bandMid == p.bandHigh;
    const bool settled = ! bandLow.isSmoothing() && ! bandMid.isSmoothing() && ! bandHigh.isSmoothing();
    splitTarget = ! (equal && settled);
    splitMix.setTargetValue (splitTarget ? 1.0f : 0.0f);

    if (snap)
    {
        splitTarget = ! equal;
        splitMix.setCurrentAndTargetValue (splitTarget ? 1.0f : 0.0f);
        reset();
    }
}

void SideBus::updateFilters() noexcept
{
    const double f = hpHz.getCurrentValue();
    const auto c = Biquad::highPass (fs, f, butterworthQ);
    for (auto& bus : hp)
        for (auto& b : bus)
            b.setCoefficients (c);
    lr1.setCutoffFrequency (xLo.getCurrentValue());
    lr2.setCutoffFrequency (xHi.getCurrentValue());
    ap2.setCutoffFrequency (xHi.getCurrentValue());
}

void SideBus::advanceWhileAsleep (int numSamples) noexcept
{
    for (auto* sv : { &hpHz, &hpMix, &xLo, &xHi, &bandLow, &bandMid, &bandHigh, &splitMix,
                      &duck, &width, &midBlend, &guardMix })
        sv->skip (numSamples);
}

void SideBus::spectralWeights (float* w, int numBins, int fftSize) const noexcept
{
    const double hpF = hpHz.getCurrentValue(), hpW = hpMix.getCurrentValue();
    const double f1 = xLo.getCurrentValue(), f2 = xHi.getCurrentValue();
    const double gL = bandLow.getCurrentValue(), gM = bandMid.getCurrentValue(), gH = bandHigh.getCurrentValue();
    const double sw = splitMix.getCurrentValue();
    // |LP_LR4|  = 1 / (1 + x^4) and |HP_LR4| = x^4 / (1 + x^4); they sum to 1.
    auto hp = [] (double f, double fc) { const double x = f / fc, x4 = x * x * x * x; return x4 / (1.0 + x4); };
    for (int b = 0; b < numBins; ++b)
    {
        const double f = (double) b * fs / fftSize;
        const double bass = (1.0 - hpW) + hpW * hp (f, hpF);
        const double h1 = hp (f, f1), h2 = hp (f, f2);
        const double split = gL * (1.0 - h1) + gM * h1 * (1.0 - h2) + gH * h1 * h2;
        w[b] = (float) (bass * ((1.0 - sw) * gL + sw * split));
    }
}

void SideBus::process (const float* sBus, const float* sPost, const float* dBus, const float* e, const float* mD,
                       float* sSyn, float* mOut, int numSamples, float* guardOut) noexcept
{
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (64, numSamples - done);
        if (hpHz.isSmoothing() || xLo.isSmoothing() || xHi.isSmoothing())
        {
            hpHz.skip (n);
            xLo.skip (n);
            xHi.skip (n);
            updateFilters();
        }

        // Whole stages are skipped when their crossfade is settled at zero.
        const bool hpRun = hpMix.isSmoothing() || hpMix.getTargetValue() > 0.0f;
        const bool splitRun = splitMix.isSmoothing() || splitMix.getTargetValue() > 0.0f;
        const bool guardRun = guardMix.isSmoothing() || guardMix.getTargetValue() > 0.0f;

        for (int i = done; i < done + n; ++i)
        {
            float s = sBus[i], d = dBus[i];

            // Step 1: bass mono.
            const float wHp = hpMix.getNextValue();
            if (hpRun)
            {
                const float sh = (float) hp[0][1].process (hp[0][0].process (s));
                const float dh = (float) hp[1][1].process (hp[1][0].process (d));
                s += wHp * (sh - s);
                d += wHp * (dh - d);
            }

            // Step 2: width bands.
            const float gL = bandLow.getNextValue(), gM = bandMid.getNextValue(), gH = bandHigh.getNextValue();
            const float wSplit = splitMix.getNextValue();
            if (splitRun)
            {
                float lowS, restS, midS, highS, lowD, restD, midD, highD;
                lr1.processSample (0, s, lowS, restS);
                lr2.processSample (0, restS, midS, highS);
                lowS = ap2.processSample (0, lowS);
                lr1.processSample (1, d, lowD, restD);
                lr2.processSample (1, restD, midD, highD);
                lowD = ap2.processSample (1, lowD);
                const float splitS = gL * lowS + gM * midS + gH * highS;
                const float splitD = lowD + midD + highD;
                s = (1.0f - wSplit) * gL * s + wSplit * splitS;
                d = (1.0f - wSplit) * d + wSplit * splitD;
            }
            else
                s *= gL;
            if (sPost != nullptr)
                s += sPost[i];

            // Step 3: transient duck.
            const float dk = duck.getNextValue();
            if (e != nullptr)
                s *= 1.0f - dk * e[i];

            // Step 4: width and correlation guard.
            const float sw = width.getNextValue() * s;
            const float dSyn = flushTiny (d);
            const float mo = mD[i] + midBlend.getNextValue() * dSyn;
            const float wG = guardMix.getNextValue();
            float g = 1.0f;
            if (guardRun)
            {
                const double pm = guardPm.process ((double) mo * mo);
                const double ps = guardPs.process ((double) sw * sw);
                const double target = ps > pm ? std::sqrt (pm / (ps + 1.0e-20)) : 1.0;
                g = 1.0f + wG * ((float) guardG.process (target) - 1.0f);
            }
            sSyn[i] = flushTiny (g * sw);
            if (guardOut != nullptr)
                guardOut[i] = g;
            mOut[i] = mo;
        }

        if (! hpRun)
            for (auto& bus : hp)
                for (auto& b : bus)
                    b.reset();
        if (! splitRun)
        {
            lr1.reset();
            lr2.reset();
            ap2.reset();
        }
        else
        {
            lr1.snapToZero();
            lr2.snapToZero();
            ap2.snapToZero();
        }
        if (! guardRun)
        {
            guardPm.reset();
            guardPs.reset();
            guardG.reset (1.0);
        }
        done += n;
    }
}
} // namespace sph
