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
    spec.assign ((size_t) k, {});
    bus.assign ((size_t) k, {});
    for (auto* v : { &mag, &mt, &mx, &mn, &ma })
        v->assign ((size_t) k, 0.0f);
    resetAll();
}

void Analysis::reset()
{
    split.reset();
    ambience.reset();
}

void Analysis::resetAll()
{
    stft.reset();
    reset();
    frames = 0;
}

void Analysis::analyseFrame (const Needs& needs) noexcept
{
    const int k = stft.numBins();
    stft.analyze (spec.data());
    for (int b = 0; b < k; ++b)
        mag[(size_t) b] = (float) (std::abs (spec[(size_t) b]) * magScale);

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
}

void Analysis::process (const float* m, int numSamples, const Needs& needs,
                        float* tonal, float* noise, float* transient) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        if (stft.pushInput (m[i]) && needs.stages)
            analyseFrame (needs);

        // Every channel is popped every sample so no stale slot survives.
        const float t = stft.popOutput (chTonal);
        const float nz = stft.popOutput (chNoise);
        stft.popOutput (chPan);
        const float x = stft.popOutput (chTransient);
        if (tonal != nullptr)
            tonal[i] = t;
        if (noise != nullptr)
            noise[i] = nz;
        if (transient != nullptr)
            transient[i] = x;
    }
}
} // namespace sph
