#include "dsp/Analysis.h"

#include <algorithm>
#include <cmath>

namespace sph
{
void Analysis::prepare (double sampleRate)
{
    fs = sampleRate;
    const int n = Stft::fftSizeForRate (fs);
    stft.prepare (n, n / 4, numChannels);
    const int k = stft.numBins();
    magScale = 2.0 / stft.windowSum();
    split.prepare (k);
    ambience.prepare (k, fs, stft.hopSize());
    pans.prepare (fs, n, stft.hopSize());
    coh.prepare (fs, n, stft.hopSize());
    stftD.prepare (n, n / 4, 0);
    specD.assign ((size_t) k, {});
    image.prepare (fs, n, stft.hopSize());
    stftS.prepare (n, n / 4, 1);
    specS.assign ((size_t) k, {});
    spec.assign ((size_t) k, {});
    bus.assign ((size_t) k, {});
    for (auto* v : { &mag, &mt, &mx, &mn, &ma, &prevLog })
        v->assign ((size_t) k, 0.0f);
    fluxLo = std::max (1, (int) std::ceil (40.0 * n / fs));
    fluxHi = std::min (k - 1, (int) std::floor (12000.0 * n / fs));
    resetAll();
}

void Analysis::reset()
{
    split.reset();
    ambience.reset();
    std::fill (prevLog.begin(), prevLog.end(), 0.0f);
    fluxHistory.fill (0.0f);
    fluxPos = 0;
    fluxValue = 0.0f;
}

void Analysis::resetAll()
{
    stft.reset();
    stftD.reset();
    stftS.reset();
    image.reset();
    reset();
    pans.reset();
    coh.reset();
    frames = 0;
}

void Analysis::detectOnset (int index) noexcept
{
    // Mean positive log-spectral flux from 40 Hz to 12 kHz, against the
    // median of the last 16 frames times 1.5 plus 0.02.
    double f = 0.0;
    for (int b = fluxLo; b <= fluxHi; ++b)
    {
        const float l = std::log1p (100.0f * mag[(size_t) b]);
        f += std::max (0.0f, l - prevLog[(size_t) b]);
        prevLog[(size_t) b] = l;
    }
    fluxValue = (float) (f / (fluxHi - fluxLo + 1));
    // All 16 slots, zeros included: a reset history then equals the history
    // after a long silence, so sleeping changes nothing (T17).
    std::array<float, 16> sorted = fluxHistory;
    std::nth_element (sorted.begin(), sorted.begin() + 8, sorted.end());
    const float threshold = 1.5f * sorted[8] + 0.02f;
    if (fluxValue > threshold && onsetCount < maxOnsets)
        onsets[(size_t) onsetCount++] = { index, std::min (1.0f, 2.0f * (fluxValue - threshold) / threshold) };
    fluxHistory[(size_t) fluxPos] = fluxValue;
    fluxPos = (fluxPos + 1) % 16;
}

void Analysis::analyseFrame (const Needs& needs, int index) noexcept
{
    const int k = stft.numBins();
    stft.analyze (spec.data());
    for (int b = 0; b < k; ++b)
        mag[(size_t) b] = (float) (std::abs (spec[(size_t) b]) * magScale);
    if (needs.flux)
        detectOnset (index);

    split.process (mag.data(), mt.data(), mx.data(), mn.data());
    ambience.process (mag.data(), mt.data(), mn.data(), ma.data(), amb, decay);
    ++frames;

    auto synth = [&] (Channel ch, const std::vector<float>& mask)
    {
        for (int b = 0; b < k; ++b)
            bus[(size_t) b] = spec[(size_t) b] * mask[(size_t) b];
        stft.synthesize (ch, bus.data());
    };
    if (needs.tonal)
        synth (chTonal, mt);
    if (needs.noise)
        synth (chNoise, mn);
    if (needs.transient)
        synth (chTransient, mx);
    if (needs.pan)
    {
        pans.processFrame (spec.data(), mag.data(), mt.data(), mn.data(), bus.data());
        stft.synthesize (chPan, bus.data());
    }
    if (needs.expander)
    {
        stftS.analyze (specS.data());
        image.processFrame (spec.data(), specS.data(), bus.data());
        stftS.synthesize (0, bus.data());
    }
    if (needs.coherence)
    {
        stftD.analyze (specD.data());
        coh.processFrame (spec.data(), specD.data(), mt.data(), mn.data(), mx.data(), bus.data());
        stft.synthesize (chCoherence, bus.data());
    }
}

void Analysis::process (const float* m, int numSamples, const Needs& needs,
                        float* tonal, float* noise, float* transient, float* pan, float* coherenceSide,
                        const float* sideIn, float* sideOut) noexcept
{
    onsetCount = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        // The decorrelated copy runs on the same frame grid as the input.
        stftD.pushInput (needs.coherence ? coh.decorrelate (m[i]) : 0.0f);
        stftS.pushInput (sideIn != nullptr ? sideIn[i] : 0.0f);
        const float sx = stftS.popOutput (0);
        if (sideOut != nullptr)
            sideOut[i] = sx;
        if (stft.pushInput (m[i]) && needs.stages)
            analyseFrame (needs, i);

        // Every channel is popped every sample so no stale slot survives.
        const float t = stft.popOutput (chTonal);
        const float nz = stft.popOutput (chNoise);
        const float pn = stft.popOutput (chPan);
        const float ch = stft.popOutput (chCoherence);
        if (coherenceSide != nullptr)
            coherenceSide[i] = ch;
        const float x = stft.popOutput (chTransient);
        if (tonal != nullptr)
            tonal[i] = t;
        if (noise != nullptr)
            noise[i] = nz;
        if (transient != nullptr)
            transient[i] = x;
        if (pan != nullptr)
            pan[i] = pn;
    }
}
} // namespace sph
