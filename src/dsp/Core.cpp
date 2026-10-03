#include "dsp/Core.h"

#include "dsp/PeakScanner.h"
#include "dsp/Stft.h"

#include <algorithm>
#include <cmath>

namespace sph
{
Core::Core() = default;
Core::~Core() = default;

int Core::latencyFor (Engine e, double sampleRate) noexcept
{
    return e == Engine::Full ? Stft::fftSizeForRate (sampleRate) : 0;
}

void Core::prepare (const ProcessSpec& s, const Params& p)
{
    spec = s;
    spec.maxBlockSize = std::max (1, spec.maxBlockSize);
    const double fs = spec.sampleRate;
    fftSize = Stft::fftSizeForRate (fs);

    alignM.prepare (fftSize);
    alignS.prepare (fftSize);
    eDelay.prepare (fftSize);
    detector.prepare (fs);
    analyser.prepare (fs);
    panFader.prepare (fs);
    spreadGen.prepare (spec);
    haasGen.prepare (spec);
    modGen.prepare (spec);
    velvetGen.prepare (spec);
    dblGen.prepare (spec);
    roomGen.prepare (spec);
    expanderMix.reset (fs, 0.030);
    for (auto& slot : slots)
        slot.amount.reset (fs, 0.020);
    side.prepare (spec);
    out.prepare (spec);
    meters.prepare (fs);

    for (auto* v : { &m, &sIn, &mD, &sInD, &e, &tonal, &noise, &tonalNoise, &panSide, &panPost, &cohSide, &sExp, &sBus, &dBus, &sTmp, &mTmp, &sSyn, &mOut, &outTmpL, &outTmpR })
        v->assign ((size_t) spec.maxBlockSize, 0.0f);

    engineFadeLen = std::max (1, (int) std::lround (0.020 * fs));
    params = p;
    snapRequested.store (false);
    applySnap (p);
    meters.reset();
}

void Core::reset()
{
    applySnap (params);
}

void Core::applySnap (const Params& p)
{
    engine = p.engine;
    const int newLatency = latencyFor (engine, spec.sampleRate);
    if (newLatency != latency)
        latencyChanged.store (true);
    latency = newLatency;
    engineSwitching = false;
    engineFadePos = engineFadeLen;

    panMode = p.panMode;
    panFader.snap();
    spreadGen.setParams (p, true);
    haasGen.setParams (p, true);
    modGen.setParams (p, true);
    velvetGen.setParams (p, true);
    dblGen.setParams (p, true);
    roomGen.setParams (p, true);
    expanderMix.setCurrentAndTargetValue (p.imgAmount != 1.0f || p.imgDiffuse != 1.0f ? 1.0f : 0.0f);
    side.setParams (p, true);
    out.setParams (p, true);
    const float amounts[numGenerators] = { p.spreadAmount, p.delayAmount, p.modAmount, p.velvetAmount, p.panAmount,
                                           p.cohAmount, p.dblAmount, p.roomAmount };
    for (int g = 0; g < numGenerators; ++g)
        slots[(size_t) g].amount.setCurrentAndTargetValue (amounts[g]);
    resetAll();
}

void Core::resetAll()
{
    alignM.reset();
    alignS.reset();
    eDelay.reset();
    detector.reset();
    detectorSleep.reset();
    analyser.resetAll();
    analyserSleep.reset();
    spreadGen.reset();
    haasGen.reset();
    modGen.reset();
    velvetGen.reset();
    dblGen.reset();
    roomGen.reset();
    expanderMix.setCurrentAndTargetValue (expanderMix.getTargetValue());
    for (auto& slot : slots)
    {
        slot.amount.setCurrentAndTargetValue (slot.amount.getTargetValue());
        slot.sleep.reset();
    }
    side.reset();
    sideSleep.reset();
    out.reset();
}

int Core::samplesUntilEngineSwitch() const noexcept
{
    return engineSwitching ? engineFadePos : spec.maxBlockSize;
}

void Core::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
{
    if (snapRequested.exchange (false))
        applySnap (params);
    else
    {
        spreadGen.setParams (params, false);
        haasGen.setParams (params, false);
        modGen.setParams (params, false);
        velvetGen.setParams (params, false);
        dblGen.setParams (params, false);
        roomGen.setParams (params, false);
        expanderMix.setTargetValue (params.imgAmount != 1.0f || params.imgDiffuse != 1.0f ? 1.0f : 0.0f);
        side.setParams (params, false);
        out.setParams (params, false);
        const float amounts[numGenerators] = { params.spreadAmount, params.delayAmount, params.modAmount, params.velvetAmount,
                                               params.panAmount, params.cohAmount, params.dblAmount, params.roomAmount };
        for (int g = 0; g < numGenerators; ++g)
            slots[(size_t) g].amount.setTargetValue (amounts[g]);
        if (params.engine != engine)
            engineSwitching = true;
        if (params.panMode != panMode)
            panFader.request();
    }

    int done = 0;
    while (done < numSamples)
    {
        if (engineSwitching && engineFadePos == 0)
        {
            // Faded out: switch, reset every module and the alignment delay,
            // then fade back in.
            engineSwitching = false;
            if (params.engine != engine)
            {
                engine = params.engine;
                latency = latencyFor (engine, spec.sampleRate);
                resetAll();
                latencyChanged.store (true);
            }
        }
        const int n = std::min ({ numSamples - done, spec.maxBlockSize,
                                  engineSwitching ? engineFadePos : spec.maxBlockSize });
        processChunk (inL + done, inR != nullptr ? inR + done : nullptr, outL + done, outR + done, n);
        done += n;
    }
}

void Core::processChunk (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    const double fs = spec.sampleRate;
    const bool full = engine == Engine::Full;

    // 1. Input split.
    if (inR != nullptr)
        for (int i = 0; i < n; ++i)
        {
            m[(size_t) i] = 0.5f * (inL[i] + inR[i]);
            sIn[(size_t) i] = 0.5f * (inL[i] - inR[i]);
        }
    else
        for (int i = 0; i < n; ++i)
        {
            m[(size_t) i] = inL[i];
            sIn[(size_t) i] = 0.0f;
        }

    // 2. Alignment delay, an integer delay of exactly the latency.
    for (int i = 0; i < n; ++i)
    {
        alignM.push (m[(size_t) i]);
        alignS.push (sIn[(size_t) i]);
        mD[(size_t) i] = alignM.readInt (latency);
        sInD[(size_t) i] = alignS.readInt (latency);
    }
    const float peakM = PeakScanner::peak (m.data(), n);
    const float peakMd = PeakScanner::peak (mD.data(), n);

    // 3. Sleep decisions for the generators.
    const bool widthZero = side.widthIsZeroAndSettled();
    bool allZeroGain = true;
    auto decide = [&] (GeneratorId g, Source activeSrc, Source pendingSrc, int tailSamples, bool settled)
    {
        auto& slot = slots[(size_t) g];
        const bool zeroGain = widthZero || (settled && ! slot.amount.isSmoothing() && slot.amount.getTargetValue() == 0.0f);
        allZeroGain = allZeroGain && zeroGain;
        // A spectral bus at time n depends on the mid up to 2N samples back,
        // so its silence is judged on the undelayed mid.
        const bool spectral = full && (isSpectral (activeSrc) || isSpectral (pendingSrc));
        const float pk = spectral ? peakM : peakMd;
        const int64_t required = tailSamples + (spectral ? 2 * (int64_t) fftSize : 0);
        slot.awake = slot.sleep.update (zeroGain, pk, n, required, forceAwake);
        return slot.sleep.justFellAsleep();
    };
    if (decide (genSpread, spreadGen.active().source, spreadGen.pending().source, spreadGen.tailSamples(), spreadGen.isSettled()))
        spreadGen.reset();
    if (decide (genDelay, haasGen.active().source, haasGen.pending().source, haasGen.tailSamples(), haasGen.isSettled()))
        haasGen.reset();
    if (decide (genMod, modGen.active().source, modGen.pending().source, modGen.tailSamples(), modGen.isSettled()))
        modGen.reset();
    if (decide (genVelvet, velvetGen.active().source, velvetGen.pending().source, velvetGen.tailSamples(), velvetGen.isSettled()))
        velvetGen.reset();
    // Pan map: Full engine only, and judged on the undelayed mid like the
    // analysis it is part of. A pending mode change is applied between blocks.
    if (panFader.readyToApply())
    {
        panMode = params.panMode;
        analyser.resetPanMap();
        panFader.applied();
    }
    {
        auto& slot = slots[genPan];
        const bool zeroGain = ! full || widthZero
                              || (panFader.isSettled() && ! slot.amount.isSmoothing() && slot.amount.getTargetValue() == 0.0f);
        allZeroGain = allZeroGain && zeroGain;
        slot.awake = slot.sleep.update (zeroGain, peakM, n, fftSize + (int64_t) std::lround (0.5 * fs),
                                        forceAwake && full);
        if (slot.sleep.justFellAsleep())
            analyser.resetPanMap();
    }
    // Coherence designer: Full engine only, judged like the Pan map.
    {
        auto& slot = slots[genCoherence];
        const bool zeroGain = ! full || widthZero || (! slot.amount.isSmoothing() && slot.amount.getTargetValue() == 0.0f);
        allZeroGain = allZeroGain && zeroGain;
        slot.awake = slot.sleep.update (zeroGain, peakM, n, fftSize + (int64_t) std::lround (0.5 * fs),
                                        forceAwake && full);
        if (slot.sleep.justFellAsleep())
            analyser.resetCoherence();
    }
    if (decide (genDouble, dblGen.active().source, dblGen.pending().source, dblGen.tailSamples(), dblGen.isSettled()))
        dblGen.reset();
    if (decide (genRoom, roomGen.activeSource(), roomGen.activeSource(), roomGen.tailSamples(), roomGen.isSettled()))
        roomGen.reset();

    // 4. Analysis and buses. In the Light engine every bus is the mid.
    Buses buses { mD.data(), mD.data(), mD.data(), mD.data() };
    expanderOn = false;
    if (! full)
        expanderMix.skip (n);
    if (full)
    {
        bool needTonal = false, needNoise = false;
        auto need = [&] (GeneratorId g, Source a, Source b)
        {
            if (! slots[(size_t) g].awake)
                return;
            needTonal = needTonal || usesTonal (a) || usesTonal (b);
            needNoise = needNoise || usesNoise (a) || usesNoise (b);
        };
        need (genSpread, spreadGen.active().source, spreadGen.pending().source);
        need (genDelay, haasGen.active().source, haasGen.pending().source);
        need (genMod, modGen.active().source, modGen.pending().source);
        need (genVelvet, velvetGen.active().source, velvetGen.pending().source);
        need (genDouble, dblGen.active().source, dblGen.pending().source);
        need (genRoom, roomGen.activeSource(), roomGen.activeSource());

        // Image expander: stereo input only, while its settings are not neutral.
        expanderOn = inR != nullptr && (expanderMix.isSmoothing() || expanderMix.getTargetValue() > 0.0f);
        const float peakS = expanderOn ? PeakScanner::peak (sIn.data(), n) : 0.0f;

        // The whole analysis sleeps when nobody consumes it, or when the mid
        // has been silent for N samples plus 0.5 s.
        const bool panAwake = slots[genPan].awake;
        const bool cohAwake = slots[genCoherence].awake;
        const bool wanted = needTonal || needNoise || panAwake || cohAwake || expanderOn;
        const bool stages = analyserSleep.update (! wanted, std::max (peakM, peakS), n,
                                                  fftSize + (int64_t) std::lround (0.5 * fs), forceAwake);
        if (analyserSleep.justFellAsleep())
            analyser.reset();
        analyser.setParams (params.ambience, params.roomDecayS);
        analyser.panMap().setParams (panMode, params.panDepth, params.panDensity, params.panBassCenterHz,
                                     params.panMaxGroups);
        if (stages && panAwake)
            side.spectralWeights (analyser.panMap().weights(), analyser.numBins(), analyser.fftSize());
        analyser.coherence().setParams (params);
        analyser.expander().setParams (params);
        const Analysis::Needs needs { stages, stages && needTonal, stages && needNoise, false, stages && panAwake,
                                      stages && cohAwake, stages && expanderOn };
        analyser.process (m.data(), n, needs, tonal.data(), noise.data(), nullptr, panSide.data(), cohSide.data(),
                          expanderOn ? sIn.data() : nullptr, sExp.data());
        if (expanderOn)
            for (int i = 0; i < n; ++i)
            {
                const float w = expanderMix.getNextValue();
                sInD[(size_t) i] = (1.0f - w) * sInD[(size_t) i] + w * sExp[(size_t) i];
            }
        else
            expanderMix.skip (n);
        if (needTonal && needNoise)
            for (int i = 0; i < n; ++i)
                tonalNoise[(size_t) i] = tonal[(size_t) i] + noise[(size_t) i];
        buses = { mD.data(), tonal.data(), noise.data(), tonalNoise.data() };
    }

    // 5. Transient detector.
    const bool detAwake = detectorSleep.update (side.duckIsZeroAndSettled() || allZeroGain, peakM, n,
                                                latency + (int64_t) std::lround (0.3 * fs), forceAwake);
    if (detAwake)
        detector.process (m.data(), e.data(), n);
    else
    {
        if (detectorSleep.justFellAsleep())
            detector.reset();
        std::fill (e.begin(), e.begin() + n, 0.0f);
    }
    if (full)
    {
        const int eLag = latency - (int) std::lround (0.002 * fs);
        for (int i = 0; i < n; ++i)
        {
            eDelay.push (e[(size_t) i]);
            e[(size_t) i] = eDelay.readInt (eLag);
        }
    }

    // 6. Generators, summed onto the side and mid-difference buses.
    std::fill (sBus.begin(), sBus.begin() + n, 0.0f);
    std::fill (dBus.begin(), dBus.begin() + n, 0.0f);
    auto accumulate = [&] (Slot& slot, const float* s, const float* md)
    {
        for (int i = 0; i < n; ++i)
        {
            const float a = slot.amount.getNextValue();
            sBus[(size_t) i] += a * s[i];
            if (md != nullptr)
                dBus[(size_t) i] += a * md[i];
        }
    };
    awakeMask = 0;
    if (slots[genSpread].awake)
    {
        spreadGen.process (buses, sTmp.data(), n);
        accumulate (slots[genSpread], sTmp.data(), nullptr);
        awakeMask |= 1u << genSpread;
    }
    else
        slots[genSpread].amount.skip (n);
    if (slots[genDelay].awake)
    {
        haasGen.process (buses, sTmp.data(), mTmp.data(), n);
        accumulate (slots[genDelay], sTmp.data(), mTmp.data());
        awakeMask |= 1u << genDelay;
    }
    else
        slots[genDelay].amount.skip (n);
    if (slots[genMod].awake)
    {
        modGen.process (buses, sTmp.data(), mTmp.data(), n);
        accumulate (slots[genMod], sTmp.data(), mTmp.data());
        awakeMask |= 1u << genMod;
    }
    else
    {
        modGen.advanceWhileAsleep (n);
        slots[genMod].amount.skip (n);
    }
    if (slots[genVelvet].awake)
    {
        velvetGen.process (buses, sTmp.data(), mTmp.data(), n);
        accumulate (slots[genVelvet], sTmp.data(), mTmp.data());
        awakeMask |= 1u << genVelvet;
    }
    else
        slots[genVelvet].amount.skip (n);

    if (slots[genDouble].awake)
    {
        dblGen.process (buses, sTmp.data(), mTmp.data(), n);
        accumulate (slots[genDouble], sTmp.data(), mTmp.data());
        awakeMask |= 1u << genDouble;
    }
    else
    {
        dblGen.advanceWhileAsleep (n);
        slots[genDouble].amount.skip (n);
    }
    if (slots[genRoom].awake)
    {
        roomGen.process (buses, sTmp.data(), mTmp.data(), n);
        accumulate (slots[genRoom], sTmp.data(), mTmp.data());
        awakeMask |= 1u << genRoom;
    }
    else
        slots[genRoom].amount.skip (n);
    if (slots[genCoherence].awake)
    {
        accumulate (slots[genCoherence], cohSide.data(), nullptr);
        awakeMask |= 1u << genCoherence;
    }
    else
        slots[genCoherence].amount.skip (n);

    // The Pan map joins the side bus after its filters (they are already
    // applied in the spectrum, without phase shift).
    const bool panOut = slots[genPan].awake;
    if (panOut)
    {
        auto& slot = slots[genPan];
        for (int i = 0; i < n; ++i)
            panPost[(size_t) i] = panSide[(size_t) i] * panFader.next() * slot.amount.getNextValue();
        awakeMask |= 1u << genPan;
    }
    else
    {
        slots[genPan].amount.skip (n);
        for (int i = 0; i < n; ++i)
            panFader.next();
    }

    // 7. Side bus.
    const bool allAsleep = awakeMask == 0;
    const float sidePeak = allAsleep ? std::max (PeakScanner::peak (sBus.data(), n), PeakScanner::peak (dBus.data(), n)) : 1.0f;
    const bool sideAwake = sideSleep.update (false, sidePeak, n, side.tailSamples(), forceAwake);
    if (sideAwake)
        side.process (sBus.data(), panOut ? panPost.data() : nullptr, dBus.data(), e.data(), mD.data(),
                      sSyn.data(), mOut.data(), n);
    else
    {
        if (sideSleep.justFellAsleep())
            side.reset();
        side.advanceWhileAsleep (n);
        std::fill (sSyn.begin(), sSyn.begin() + n, 0.0f);
        std::copy (mD.begin(), mD.begin() + n, mOut.begin());
    }

    // 8. Output stage.
    out.process (mOut.data(), sSyn.data(), mD.data(), sInD.data(), outL, outR, n);

    // Engine switch fade of the whole output.
    if (engineSwitching || engineFadePos < engineFadeLen)
    {
        const float step = 1.0f / (float) engineFadeLen;
        for (int i = 0; i < n; ++i)
        {
            engineFadePos = engineSwitching ? std::max (0, engineFadePos - 1) : std::min (engineFadeLen, engineFadePos + 1);
            const float g = (float) engineFadePos * step;
            outL[i] *= g;
            outR[i] *= g;
        }
    }

    meters.process (outL, outR, n);

    // Host tail (DESIGN.md section 7.5).
    // A spectral bus outlasts its input by a further N samples.
    int longest = 0;
    auto tailOf = [&] (GeneratorId g, Source src, int t)
    {
        if (slots[(size_t) g].awake)
            longest = std::max (longest, t + (full && isSpectral (src) ? fftSize : 0));
    };
    tailOf (genSpread, spreadGen.active().source, spreadGen.tailSamples());
    tailOf (genDelay, haasGen.active().source, haasGen.tailSamples());
    tailOf (genMod, modGen.active().source, modGen.tailSamples());
    tailOf (genVelvet, velvetGen.active().source, velvetGen.tailSamples());
    tailOf (genDouble, dblGen.active().source, dblGen.tailSamples());
    tailOf (genRoom, roomGen.activeSource(), roomGen.tailSamples());
    tailOf (genPan, Source::Tonal, fftSize);
    tailOf (genCoherence, Source::Tonal, fftSize + (int) std::lround (0.021 * fs));
    if (sideAwake)
        longest = std::max (longest, side.tailSamples());
    tail.store ((double) (longest + latency) / fs, std::memory_order_relaxed);
}
} // namespace sph
