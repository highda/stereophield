#include "dsp/MaterialClassifier.h"

#include "dsp/OnePole.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sph
{
namespace
{
// 0 below a, 1 above b, smooth in between.
double smoothStep (double x, double a, double b) noexcept
{
    const double t = std::clamp ((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
} // namespace

MaterialClassifier::MaterialClassifier() = default;
MaterialClassifier::~MaterialClassifier() = default;

void MaterialClassifier::prepare (double sampleRate)
{
    fs = sampleRate;
    int order = 10;
    while ((1 << order) < 0.0213 * fs && order < 14)
        ++order;
    n = 1 << order;
    hop = n / 2;
    fft = std::make_unique<juce::dsp::FFT> (order);
    window.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * i / n));
    ring.assign ((size_t) n, 0.0f);
    work.assign ((size_t) (2 * n), 0.0f);
    const int bins = n / 2 + 1;
    mag.assign ((size_t) bins, 0.0f);
    logMag.assign ((size_t) bins, 0.0f);
    prevLog.assign ((size_t) bins, 0.0f);
    peak.assign ((size_t) bins, 0);
    prevPeak.assign ((size_t) bins, 0);
    kLo = std::max (1, (int) std::ceil (60.0 * n / fs));
    kHi = std::min (bins - 3, (int) std::floor (10000.0 * n / fs));
    const double hopRate = fs / hop;
    aRate = OnePole::coefficient (2.0, hopRate);
    aTonal = OnePole::coefficient (0.5, hopRate);
    aWeight = OnePole::coefficient (2.0, hopRate);
    reset();
}

void MaterialClassifier::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    std::fill (prevLog.begin(), prevLog.end(), 0.0f);
    std::fill (prevPeak.begin(), prevPeak.end(), (unsigned char) 0);
    fluxHistory.fill (0.0f);
    fluxCount = 0;
    flux1 = flux2 = broad1 = 0.0f;
    sinceOnset = 1000;
    ringPos = hopCount = 0;
    onsetRate = 0.0;
    tonalSmooth = 0.0;
    weights = { 0.0f, 0.0f, 1.0f };
}

void MaterialClassifier::process (const float* x, int numSamples) noexcept
{
    if (fft == nullptr)
        return;
    for (int i = 0; i < numSamples; ++i)
    {
        ring[(size_t) ringPos] = x[i];
        ringPos = (ringPos + 1) & (n - 1);
        if (++hopCount >= hop)
        {
            hopCount = 0;
            analyseFrame();
        }
    }
}

void MaterialClassifier::analyseFrame() noexcept
{
    for (int i = 0; i < n; ++i)
        work[(size_t) i] = ring[(size_t) ((ringPos + i) & (n - 1))] * window[(size_t) i];
    std::fill (work.begin() + n, work.end(), 0.0f);
    fft->performFrequencyOnlyForwardTransform (work.data(), true);

    // Magnitudes normalised so that a full-scale sine peaks near 1.
    const float norm = 4.0f / (float) n;
    double energy = 0.0;
    for (int k = kLo - 1; k <= kHi + 2; ++k)
    {
        mag[(size_t) k] = work[(size_t) k] * norm;
        if (k >= kLo && k <= kHi)
            energy += (double) mag[(size_t) k] * mag[(size_t) k];
    }
    const int bands = kHi - kLo + 1;
    // Silence (below about -80 dB per bin on average) holds everything.
    if (energy / bands < 1e-8)
    {
        std::fill (prevLog.begin(), prevLog.end(), 0.0f);
        std::fill (prevPeak.begin(), prevPeak.end(), (unsigned char) 0);
        flux1 = flux2 = broad1 = 0.0f;
        return;
    }

    // Spectral flux on log magnitudes, and how many bins rose together.
    double flux = 0.0;
    int rising = 0;
    for (int k = kLo; k <= kHi; ++k)
    {
        const float lm = std::log (1.0f + 1000.0f * mag[(size_t) k]);
        const float d = lm - prevLog[(size_t) k];
        if (d > 0.0f)
            flux += d;
        // Bins that rose minus bins that fell: near 0 for steady noise and
        // for a new note replacing an old one, large for a hit.
        rising += d > 0.1f ? 1 : (d < -0.1f ? -1 : 0);
        prevLog[(size_t) k] = lm;
        logMag[(size_t) k] = lm;
    }
    const float f = (float) (flux / bands);
    const float broad = (float) rising / (float) bands;

    // Peak picking one frame late: the previous frame is an onset if it is a
    // local maximum above an adaptive threshold (median of the last 16).
    std::array<float, 16> sorted = fluxHistory;
    std::nth_element (sorted.begin(), sorted.begin() + 8, sorted.end());
    const float threshold = 1.5f * sorted[8] + 0.05f;
    bool onset = flux1 > flux2 && flux1 >= f && flux1 > threshold && broad1 > 0.3f && sinceOnset * hop > 0.05 * fs;
    sinceOnset = onset ? 0 : sinceOnset + 1;
    fluxHistory[(size_t) (fluxCount++ & 15)] = f;
    flux2 = flux1;
    flux1 = f;
    broad1 = broad;
    onsetRate = aRate * onsetRate + (1.0 - aRate) * (onset ? fs / hop : 0.0);

    // Tonal share: energy in stable peaks at least 6 dB above the mean power
    // of their neighbourhood (3 to 8 bins away).
    double tonalEnergy = 0.0;
    for (int k = kLo; k <= kHi; ++k)
    {
        const float p = mag[(size_t) k] * mag[(size_t) k];
        bool isPeak = p > mag[(size_t) (k - 1)] * mag[(size_t) (k - 1)] && p >= mag[(size_t) (k + 1)] * mag[(size_t) (k + 1)]
                      && p > mag[(size_t) (k + 2)] * mag[(size_t) (k + 2)] && (k < 2 || p > mag[(size_t) (k - 2)] * mag[(size_t) (k - 2)]);
        if (isPeak)
        {
            // Mean power 3 to 8 bins away, outside the window's main lobe.
            double local = 0.0;
            int count = 0;
            for (int j = std::max (kLo, k - 8); j <= std::min (kHi, k + 8); ++j)
                if (std::abs (j - k) >= 3)
                {
                    local += (double) mag[(size_t) j] * mag[(size_t) j];
                    ++count;
                }
            isPeak = count > 0 && p > 4.0 * local / count;
        }
        peak[(size_t) k] = isPeak ? 1 : 0;
    }
    for (int k = kLo; k <= kHi; ++k)
    {
        if (peak[(size_t) k] == 0)
            continue;
        if (prevPeak[(size_t) k] == 0 && prevPeak[(size_t) (k - 1)] == 0 && prevPeak[(size_t) (k + 1)] == 0)
            continue;
        for (int j = std::max (kLo, k - 1); j <= std::min (kHi, k + 1); ++j)
            tonalEnergy += (double) mag[(size_t) j] * mag[(size_t) j];
    }
    std::copy (peak.begin(), peak.end(), prevPeak.begin());
    tonalSmooth = aTonal * tonalSmooth + (1.0 - aTonal) * std::min (1.0, tonalEnergy / energy);

    const double P = smoothStep (onsetRate, 0.5, 2.5);
    const double T = smoothStep (tonalSmooth, 0.3, 0.7);
    const double raw[numWeights] = { P * (1.0 - T), T * (1.0 - P), 0.0 };
    double sum = 0.0;
    for (int w = 0; w < mixed; ++w)
    {
        weights[(size_t) w] = (float) (aWeight * weights[(size_t) w] + (1.0 - aWeight) * raw[w]);
        sum += weights[(size_t) w];
    }
    weights[mixed] = (float) std::max (0.0, 1.0 - sum);
}
} // namespace sph
