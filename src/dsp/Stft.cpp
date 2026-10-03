#include "dsp/Stft.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sph
{
Stft::Stft() = default;
Stft::~Stft() = default;

void Stft::prepare (int fftSize, int hopSize, int numSynthChannels)
{
    n = fftSize;
    hop = hopSize;
    order = 0;
    while ((1 << order) < n)
        ++order;
    jassert ((1 << order) == n);

    fft = std::make_unique<juce::dsp::FFT> (order);
    win.resize ((size_t) n);
    winSum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        win[(size_t) i] = (float) std::sqrt (0.5 * (1.0 - std::cos (2.0 * std::numbers::pi * i / n)));
        winSum += win[(size_t) i];
    }

    // The squared window overlap-adds to N / (2 H); with H = N / 4 that is 2,
    // giving the 0.5 of the specification. JUCE's real inverse transform
    // already divides by N (verified by test T1).
    synthScale = (float) (2.0 * hop / n);

    input.assign ((size_t) n, 0.0f);
    inMask = n - 1;
    work.assign ((size_t) (2 * n), 0.0f);
    accMask = 2 * n - 1;
    accum.assign ((size_t) numSynthChannels, std::vector<float> ((size_t) (2 * n), 0.0f));
    reset();
}

void Stft::reset()
{
    std::fill (input.begin(), input.end(), 0.0f);
    for (auto& a : accum)
        std::fill (a.begin(), a.end(), 0.0f);
    inPos = 0;
    outPos = 0;
    hopCount = 0;
}

void Stft::analyze (std::complex<float>* spectrum) noexcept
{
    // inPos is the oldest sample of the latest N.
    for (int i = 0; i < n; ++i)
        work[(size_t) i] = input[(size_t) ((inPos + i) & inMask)] * win[(size_t) i];
    std::fill (work.begin() + n, work.end(), 0.0f);
    fft->performRealOnlyForwardTransform (work.data(), true);
    const int k = numBins();
    for (int i = 0; i < k; ++i)
        spectrum[i] = { work[(size_t) (2 * i)], work[(size_t) (2 * i + 1)] };
}

void Stft::synthesize (int channel, const std::complex<float>* spectrum) noexcept
{
    const int k = numBins();
    for (int i = 0; i < k; ++i)
    {
        work[(size_t) (2 * i)] = spectrum[i].real();
        work[(size_t) (2 * i + 1)] = spectrum[i].imag();
    }
    std::fill (work.begin() + 2 * k, work.end(), 0.0f);
    fft->performRealOnlyInverseTransform (work.data());

    // The frame covers times n-N+1 .. n, where n is the newest time at outPos.
    float* acc = accum[(size_t) channel].data();
    const int start = outPos - n + 1;
    for (int i = 0; i < n; ++i)
        acc[(start + i) & accMask] += work[(size_t) i] * win[(size_t) i] * synthScale;
}
} // namespace sph
