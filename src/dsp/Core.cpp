#include "dsp/Core.h"

#include "dsp/PeakScanner.h"
#include "dsp/Stft.h"

#include <algorithm>
#include <type_traits>
#include <cmath>

namespace sph
{
Core::Core() = default;
Core::~Core() = default;

int Core::latencyFor (Engine e, double sampleRate, LatencyMode mode) noexcept
{
    return e == Engine::Full || mode == LatencyMode::AlwaysFull ? Stft::fftSizeForRate (sampleRate) : 0;
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
    const int switchFade = std::max (1, (int) std::lround (0.030 * fs));
    fadeTrack = (float) std::exp (-1.0 / (0.002 * fs));
    auto preparePair = [&] (auto& pair)
    {
        for (auto& g : pair.inst)
        {
            g.prepare (spec);
            g.externalSwitching = true;
        }
        pair.cur = 0;
        pair.fadeLen = switchFade;
        pair.fadePos = switchFade;
    };
    preparePair (spreads);
    preparePair (haases);
    preparePair (mods);
    preparePair (velvets);
    preparePair (doubles);
    const int hist = (int) std::lround (0.150 * fs);
    for (auto& h : history)
        h.assign ((size_t) hist, 0.0f);
    historyPos = 0;
    preIn.assign ((size_t) hist, 0.0f);
    roomGen.prepare (spec);
    expanderMix.reset (fs, 0.030);
    fullMix.reset (fs, 0.030);
    fluxMarkers.assign ((size_t) (2 * fftSize), 0.0f);
    ratioRing.assign ((size_t) fftSize, 0.0f);
    fluxHold = std::exp (-1.0 / (TransientDetector::holdSeconds * fs));
    for (auto& slot : slots)
        slot.amount.reset (fs, 0.020);
    side.prepare (spec);
    out.prepare (spec);
    meters.prepare (fs);

    for (auto* v : { &m, &sIn, &mD, &sInD, &e, &tonal, &noise, &tonalNoise, &panSide, &panPost, &cohSide, &sExp, &ratio, &preS, &preM, &sOld, &mOld, &fullW, &sBus, &dBus, &sTmp, &mTmp, &sSyn, &mOut, &outTmpL, &outTmpR })
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
    latencyMode = p.latencyMode;
    fullMix.setCurrentAndTargetValue (engine == Engine::Full ? 1.0f : 0.0f);
    fullWarmup = 0;
    const int newLatency = latencyFor (engine, spec.sampleRate, latencyMode);
    if (newLatency != latency)
        latencyChanged.store (true);
    latency = newLatency;
    engineSwitching = false;
    engineFadePos = engineFadeLen;

    panMode = p.panMode;
    panFader.snap();
    for (auto& g : spreads.inst)
        g.setParams (p, true);
    spreads.fadePos = spreads.fadeLen;
    for (auto& g : haases.inst)
        g.setParams (p, true);
    haases.fadePos = haases.fadeLen;
    for (auto& g : mods.inst)
        g.setParams (p, true);
    mods.fadePos = mods.fadeLen;
    for (auto& g : velvets.inst)
        g.setParams (p, true);
    velvets.fadePos = velvets.fadeLen;
    for (auto& g : doubles.inst)
        g.setParams (p, true);
    doubles.fadePos = doubles.fadeLen;
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
    for (auto& g : spreads.inst)
        g.reset();
    for (auto& g : haases.inst)
        g.reset();
    for (auto& g : mods.inst)
        g.reset();
    for (auto& g : velvets.inst)
        g.reset();
    for (auto& g : doubles.inst)
        g.reset();
    roomGen.reset();
    expanderMix.setCurrentAndTargetValue (expanderMix.getTargetValue());
    std::fill (fluxMarkers.begin(), fluxMarkers.end(), 0.0f);
    for (auto& h : history)
        std::fill (h.begin(), h.end(), 0.0f);
    historyPos = 0;
    std::fill (ratioRing.begin(), ratioRing.end(), 0.0f);
    inputTime = 0;
    fluxEnvelope = 0.0;
    for (auto& slot : slots)
    {
        slot.amount.setCurrentAndTargetValue (slot.amount.getTargetValue());
        slot.sleep.reset();
    }
    side.reset();
    sideSleep.reset();
    out.reset();
}

bool Core::switching (GeneratorId g) const noexcept
{
    switch (g)
    {
        case genSpread: return spreads.fading();
        case genDelay: return haases.fading();
        case genMod: return mods.fading();
        case genVelvet: return velvets.fading();
        case genDouble: return doubles.fading();
        default: return false;
    }
}

template <typename G>
void Core::preRoll (G& g, Source src)
{
    // The last 150 ms of the bus the instance will read, oldest first; its
    // output is discarded. Every generator's tail is shorter than that, so
    // its state then matches one that had been running all along.
    const int len = (int) preIn.size();
    const int b = engine == Engine::Full ? (int) src : 0;
    const auto& h = history[(size_t) b];
    for (int i = 0; i < len; ++i)
        preIn[(size_t) i] = h[(size_t) ((historyPos + i) % len)];
    for (int done = 0; done < len; done += spec.maxBlockSize)
    {
        const int m = std::min (spec.maxBlockSize, len - done);
        const float* in = preIn.data() + done;
        const Buses bus { in, in, in, in };
        if constexpr (std::is_same_v<G, Spread>)
            g.process (bus, preS.data(), m);
        else
            g.process (bus, preS.data(), preM.data(), m);
    }
}

template <typename G>
void Core::switchIfSpectral (GenPair<G>& pair, GeneratorId g)
{
    // The bus of a spectral source changes with the engine.
    if (isSpectral (pair.now().active().source) && ! slots[(size_t) g].sleep.isAsleep() && ! pair.fading())
        startSwitch (pair);
}

template <typename G>
void Core::startSwitch (GenPair<G>& pair)
{
    auto& next = pair.old();
    next.setParams (params, true);
    preRoll (next, next.active().source);
    if constexpr (std::is_same_v<G, Mod>)
        next.copyPhaseFrom (pair.now());
    if constexpr (std::is_same_v<G, DoubleTracker>)
        if (next.active().seed == pair.now().active().seed)
            next.copyDriftFrom (pair.now());
    pair.cur = 1 - pair.cur;
    pair.fadePos = 0;
    pair.pn = pair.po = pair.pc = 0.0f;
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
        for (auto& g : spreads.inst)
            g.setParams (params, false);
        for (auto& g : haases.inst)
            g.setParams (params, false);
        for (auto& g : mods.inst)
            g.setParams (params, false);
        for (auto& g : velvets.inst)
            g.setParams (params, false);
        for (auto& g : doubles.inst)
            g.setParams (params, false);
        roomGen.setParams (params, false);
        expanderMix.setTargetValue (params.imgAmount != 1.0f || params.imgDiffuse != 1.0f ? 1.0f : 0.0f);
        side.setParams (params, false);
        out.setParams (params, false);
        const float amounts[numGenerators] = { params.spreadAmount, params.delayAmount, params.modAmount, params.velvetAmount,
                                               params.panAmount, params.cohAmount, params.dblAmount, params.roomAmount };
        for (int g = 0; g < numGenerators; ++g)
            slots[(size_t) g].amount.setTargetValue (amounts[g]);
        if (params.latencyMode != latencyMode)
            engineSwitching = true; // the latency changes: full fade and reset
        else if (params.engine != engine && latencyMode == LatencyMode::AlwaysFull)
        {
            // Same latency: no global fade. The Full-only parts fade, and
            // generators reading a spectral bus switch instance.
            engine = params.engine;
            if (engine == Engine::Full)
                fullWarmup = latency; // the analysis needs one frame to become valid
            else
            {
                fullWarmup = 0;
                fullMix.setTargetValue (0.0f);
            }
            switchIfSpectral (spreads, genSpread);
            switchIfSpectral (haases, genDelay);
            switchIfSpectral (mods, genMod);
            switchIfSpectral (velvets, genVelvet);
            switchIfSpectral (doubles, genDouble);
        }
        else if (params.engine != engine)
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
            if (params.engine != engine || params.latencyMode != latencyMode)
            {
                engine = params.engine;
                latencyMode = params.latencyMode;
                latency = latencyFor (engine, spec.sampleRate, latencyMode);
                fullMix.setCurrentAndTargetValue (engine == Engine::Full ? 1.0f : 0.0f);
                fullWarmup = 0;
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
    if (fullWarmup > 0 && (fullWarmup -= n) <= 0)
    {
        fullWarmup = 0;
        fullMix.setTargetValue (1.0f);
    }
    // The Full-only parts run while they are audible or fading; fullW holds
    // their per-sample weight.
    const bool fullParts = full || fullMix.getCurrentValue() > 0.0f || fullMix.isSmoothing();
    for (int i = 0; i < n; ++i)
        fullW[(size_t) i] = fullMix.getNextValue();

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

    // 3a. Structural changes: an awake generator switches to a second,
    // pre-rolled instance and crossfades; an asleep one just takes the change.
    auto structural = [&] (auto& pair, GeneratorId g)
    {
        auto& now = pair.now();
        if (pair.fading() || now.pending() == now.active())
            return;
        if (slots[(size_t) g].sleep.isAsleep())
            now.setParams (params, true);
        else
            startSwitch (pair);
    };
    structural (spreads, genSpread);
    structural (haases, genDelay);
    structural (mods, genMod);
    structural (velvets, genVelvet);
    structural (doubles, genDouble);

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
    if (decide (genSpread, spreads.now().active().source, spreads.now().pending().source, spreads.now().tailSamples(), spreads.now().isSettled()))
        for (auto& g : spreads.inst)
        g.reset();
    if (decide (genDelay, haases.now().active().source, haases.now().pending().source, haases.now().tailSamples(), haases.now().isSettled()))
        for (auto& g : haases.inst)
        g.reset();
    if (decide (genMod, mods.now().active().source, mods.now().pending().source, mods.now().tailSamples(), mods.now().isSettled()))
        for (auto& g : mods.inst)
        g.reset();
    if (decide (genVelvet, velvets.now().active().source, velvets.now().pending().source, velvets.now().tailSamples(), velvets.now().isSettled()))
        for (auto& g : velvets.inst)
        g.reset();
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
        const bool zeroGain = ! fullParts || widthZero
                              || (panFader.isSettled() && ! slot.amount.isSmoothing() && slot.amount.getTargetValue() == 0.0f);
        allZeroGain = allZeroGain && zeroGain;
        slot.awake = slot.sleep.update (zeroGain, peakM, n, fftSize + (int64_t) std::lround (0.5 * fs),
                                        forceAwake && fullParts);
        if (slot.sleep.justFellAsleep())
            analyser.resetPanMap();
    }
    // Coherence designer: Full engine only, judged like the Pan map.
    {
        auto& slot = slots[genCoherence];
        const bool zeroGain = ! fullParts || widthZero || (! slot.amount.isSmoothing() && slot.amount.getTargetValue() == 0.0f);
        allZeroGain = allZeroGain && zeroGain;
        slot.awake = slot.sleep.update (zeroGain, peakM, n, fftSize + (int64_t) std::lround (0.5 * fs),
                                        forceAwake && fullParts);
        if (slot.sleep.justFellAsleep())
            analyser.resetCoherence();
    }
    if (decide (genDouble, doubles.now().active().source, doubles.now().pending().source, doubles.now().tailSamples(), doubles.now().isSettled()))
        for (auto& g : doubles.inst)
        g.reset();
    if (decide (genRoom, roomGen.activeSource(), roomGen.activeSource(), roomGen.tailSamples(), roomGen.isSettled()))
        roomGen.reset();

    // 4. Analysis and buses. In the Light engine every bus is the mid.
    Buses buses { mD.data(), mD.data(), mD.data(), mD.data() };
    expanderOn = false;
    if (! fullParts)
        expanderMix.skip (n);
    if (fullParts)
    {
        bool needTonal = false, needNoise = false;
        auto need = [&] (GeneratorId g, Source a, Source b)
        {
            if (! slots[(size_t) g].awake)
                return;
            needTonal = needTonal || usesTonal (a) || usesTonal (b);
            needNoise = needNoise || usesNoise (a) || usesNoise (b);
        };
        need (genSpread, spreads.now().active().source, spreads.now().pending().source);
        need (genDelay, haases.now().active().source, haases.now().pending().source);
        need (genMod, mods.now().active().source, mods.now().pending().source);
        need (genVelvet, velvets.now().active().source, velvets.now().pending().source);
        need (genDouble, doubles.now().active().source, doubles.now().pending().source);
        // While crossfading, the outgoing instance still reads its bus.
        if (spreads.fading())
            need (genSpread, spreads.old().active().source, spreads.old().active().source);
        if (haases.fading())
            need (genDelay, haases.old().active().source, haases.old().active().source);
        if (mods.fading())
            need (genMod, mods.old().active().source, mods.old().active().source);
        if (velvets.fading())
            need (genVelvet, velvets.old().active().source, velvets.old().active().source);
        if (doubles.fading())
            need (genDouble, doubles.old().active().source, doubles.old().active().source);
        need (genRoom, roomGen.activeSource(), roomGen.activeSource());

        // Image expander: stereo input only, while its settings are not neutral.
        expanderOn = inR != nullptr && (expanderMix.isSmoothing() || expanderMix.getTargetValue() > 0.0f);
        const float peakS = expanderOn ? PeakScanner::peak (sIn.data(), n) : 0.0f;

        // The whole analysis sleeps when nobody consumes it, or when the mid
        // has been silent for N samples plus 0.5 s.
        const bool panAwake = slots[genPan].awake;
        const bool cohAwake = slots[genCoherence].awake;
        const bool fluxWanted = params.transientMode == TransientMode::SpectralFlux && ! side.duckIsZeroAndSettled() && ! allZeroGain;
        const bool wanted = needTonal || needNoise || panAwake || cohAwake || expanderOn || fluxWanted;
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
                                      stages && cohAwake, stages && expanderOn,
                                      stages && params.transientMode == TransientMode::SpectralFlux };
        analyser.process (m.data(), n, needs, tonal.data(), noise.data(), nullptr, panSide.data(), cohSide.data(),
                          expanderOn ? sIn.data() : nullptr, sExp.data());
        if (expanderOn)
            for (int i = 0; i < n; ++i)
            {
                const float w = expanderMix.getNextValue() * fullW[(size_t) i];
                sInD[(size_t) i] = (1.0f - w) * sInD[(size_t) i] + w * sExp[(size_t) i];
            }
        else
            expanderMix.skip (n);
        if (needTonal && needNoise)
            for (int i = 0; i < n; ++i)
                tonalNoise[(size_t) i] = tonal[(size_t) i] + noise[(size_t) i];
        if (full)
            buses = { mD.data(), tonal.data(), noise.data(), tonalNoise.data() };
    }

    // Bus history for pre-rolling switched instances.
    {
        const int len = (int) history[0].size();
        const float* src[4] = { buses.full, buses.tonal, buses.noise, buses.tonalNoise };
        for (int i = 0; i < n; ++i)
        {
            const int at = (historyPos + i) % len;
            for (int b = 0; b < 4; ++b)
                history[(size_t) b][(size_t) at] = src[b][i];
        }
        historyPos = (historyPos + n) % len;
    }

    // 5. Transient detector.
    const bool detAwake = detectorSleep.update (side.duckIsZeroAndSettled() || allZeroGain, peakM, n,
                                                latency + (int64_t) std::lround (0.3 * fs), forceAwake);
    if (detAwake)
        detector.process (m.data(), e.data(), n, ratio.data());
    else
    {
        if (detectorSleep.justFellAsleep())
            detector.reset();
        std::fill (e.begin(), e.begin() + n, 0.0f);
        std::fill (ratio.begin(), ratio.begin() + n, 0.0f);
    }
    const int ringN = (int) ratioRing.size(), markN = (int) fluxMarkers.size();
    for (int i = 0; i < n; ++i)
        ratioRing[(size_t) ((inputTime + i) % ringN)] = ratio[(size_t) i];
    if (full && params.transientMode == TransientMode::SpectralFlux)
    {
        // Each onset of the flux detector is placed at the sample of the
        // largest detector ratio in the newest three hops (an onset raises the
        // flux for two or three frames as it moves into the window, so every
        // detection of it finds the same sample), and starts the duck 2 ms
        // before that sample reaches the output.
        const int hop = analyser.hopSize(), lead = (int) std::lround (0.002 * fs);
        for (int o = 0; o < analyser.numOnsets(); ++o)
        {
            const auto& on = analyser.onset (o);
            const int64_t end = inputTime + on.index;
            int64_t at = end;
            float best = -1.0f;
            for (int64_t j = end - 3 * hop + 1; j <= end; ++j)
                if (j >= 0 && ratioRing[(size_t) (j % ringN)] > best)
                {
                    best = ratioRing[(size_t) (j % ringN)];
                    at = j;
                }
            auto& mk = fluxMarkers[(size_t) ((at + latency - lead) % markN)];
            mk = std::max (mk, on.strength);
        }
        for (int i = 0; i < n; ++i)
        {
            auto& mk = fluxMarkers[(size_t) ((inputTime + i) % markN)];
            fluxEnvelope = std::max ((double) mk, fluxEnvelope * fluxHold);
            if (fluxEnvelope < 1e-30)
                fluxEnvelope = 0.0;
            mk = 0.0f;
            e[(size_t) i] = (float) fluxEnvelope;
        }
    }
    else if (latency > 0)
    {
        const int eLag = latency - (int) std::lround (0.002 * fs);
        for (int i = 0; i < n; ++i)
        {
            eDelay.push (e[(size_t) i]);
            e[(size_t) i] = eDelay.readInt (eLag);
        }
    }
    inputTime += n;
    {
        float sum = 0.0f, mx = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            sum += e[(size_t) i];
            mx = std::max (mx, e[(size_t) i]);
        }
        envMean = sum / (float) n;
        envMax = mx;
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
    // Runs a generator pair: the current instance, and while crossfading the
    // outgoing one, blended with equal power.
    auto runPair = [&] (auto& pair, GeneratorId g, bool hasMid, auto&& proc)
    {
        auto& slot = slots[(size_t) g];
        if (! slot.awake)
        {
            slot.amount.skip (n);
            pair.fadePos = pair.fadeLen;
            return;
        }
        proc (pair.now(), sTmp.data(), mTmp.data());
        if (pair.fading())
        {
            // Linear crossfade with a power correction tracked per sample
            // (2 ms one-poles of both powers and their cross term): the mix's
            // power is steered to the linear interpolation of the two powers,
            // which is equal-power for uncorrelated outputs, equal-gain for
            // identical ones, and fills the short notches of outputs that are
            // delayed copies of each other.
            proc (pair.old(), sOld.data(), mOld.data());
            const float k = fadeTrack;
            for (int i = 0; i < n; ++i)
            {
                const float x = std::min (1.0f, (float) (pair.fadePos + i) / (float) pair.fadeLen);
                const float a = sTmp[(size_t) i], b = sOld[(size_t) i];
                pair.pn = k * pair.pn + (1.0f - k) * a * a;
                pair.po = k * pair.po + (1.0f - k) * b * b;
                pair.pc = k * pair.pc + (1.0f - k) * a * b;
                const float mixP = x * x * pair.pn + (1 - x) * (1 - x) * pair.po + 2 * x * (1 - x) * pair.pc;
                const float wanted = x * pair.pn + (1 - x) * pair.po;
                const float g = mixP > 1e-20f ? std::clamp (std::sqrt (wanted / mixP), 0.25f, 4.0f) : 1.0f;
                sTmp[(size_t) i] = g * (x * a + (1.0f - x) * b);
                mTmp[(size_t) i] = g * (x * mTmp[(size_t) i] + (1.0f - x) * mOld[(size_t) i]);
            }
            pair.fadePos = std::min (pair.fadeLen, pair.fadePos + n);
        }
        accumulate (slot, sTmp.data(), hasMid ? mTmp.data() : nullptr);
        awakeMask |= 1u << g;
    };
    runPair (spreads, genSpread, false, [&] (Spread& gen, float* s, float* md)
    {
        gen.process (buses, s, n);
        std::fill (md, md + n, 0.0f);
    });
    runPair (haases, genDelay, true, [&] (HaasDelay& gen, float* s, float* md) { gen.process (buses, s, md, n); });
    runPair (mods, genMod, true, [&] (Mod& gen, float* s, float* md) { gen.process (buses, s, md, n); });
    if (! slots[genMod].awake)
        for (auto& g : mods.inst)
            g.advanceWhileAsleep (n);
    runPair (velvets, genVelvet, true, [&] (Velvet& gen, float* s, float* md) { gen.process (buses, s, md, n); });
    runPair (doubles, genDouble, true, [&] (DoubleTracker& gen, float* s, float* md) { gen.process (buses, s, md, n); });
    if (! slots[genDouble].awake)
        for (auto& g : doubles.inst)
            g.advanceWhileAsleep (n);
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
        for (int i = 0; i < n; ++i)
            cohSide[(size_t) i] *= fullW[(size_t) i];
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
            panPost[(size_t) i] = panSide[(size_t) i] * panFader.next() * slot.amount.getNextValue() * fullW[(size_t) i];
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
    tailOf (genSpread, spreads.now().active().source, spreads.now().tailSamples());
    tailOf (genDelay, haases.now().active().source, haases.now().tailSamples());
    tailOf (genMod, mods.now().active().source, mods.now().tailSamples());
    tailOf (genVelvet, velvets.now().active().source, velvets.now().tailSamples());
    tailOf (genDouble, doubles.now().active().source, doubles.now().tailSamples());
    tailOf (genRoom, roomGen.activeSource(), roomGen.tailSamples());
    tailOf (genPan, Source::Tonal, fftSize);
    tailOf (genCoherence, Source::Tonal, fftSize + (int) std::lround (0.021 * fs));
    if (sideAwake)
        longest = std::max (longest, side.tailSamples());
    tail.store ((double) (longest + latency) / fs, std::memory_order_relaxed);
}
} // namespace sph
