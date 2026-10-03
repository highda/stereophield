#include "dsp/StereoBands.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>

namespace sph
{
StereoBands::StereoBands() = default;
StereoBands::~StereoBands() = default;

double StereoBands::bandCentre (int b) noexcept
{
    return 1000.0 * std::pow (2.0, (b - 12) / 3.0); // 63 Hz .. 12.5 kHz
}

void StereoBands::prepare (double sampleRate, double smoothingSeconds)
{
    fs = sampleRate;
    fft = std::make_unique<juce::dsp::FFT> (14);
    window.resize (frameSize);
    for (int i = 0; i < frameSize; ++i)
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / frameSize));
    for (auto* v : { &bl, &br, &bm })
        v->assign (2 * frameSize, 0.0f);
    const double hop = frameSize / 2.0 / fs;
    keep = (float) std::exp (-hop / smoothingSeconds);
    for (int b = 0; b <= numBands; ++b)
    {
        const double edge = bandCentre (b) * std::pow (2.0, -1.0 / 6.0);
        edges[(size_t) b] = std::max (1, (int) std::lround (edge * frameSize / fs));
    }
    reset();
}

void StereoBands::reset()
{
    for (auto* a : { &slr, &sll, &srr, &ssum, &sref })
        a->fill (0.0);
    corr.fill (0.0f);
    fold.fill (0.0f);
}

void StereoBands::addFrame (const float* l, const float* r, const float* ref)
{
    for (int i = 0; i < frameSize; ++i)
    {
        bl[(size_t) i] = l[i] * window[(size_t) i];
        br[(size_t) i] = r[i] * window[(size_t) i];
        bm[(size_t) i] = ref != nullptr ? ref[i] * window[(size_t) i] : 0.0f;
    }
    std::fill (bl.begin() + frameSize, bl.end(), 0.0f);
    std::fill (br.begin() + frameSize, br.end(), 0.0f);
    std::fill (bm.begin() + frameSize, bm.end(), 0.0f);
    fft->performRealOnlyForwardTransform (bl.data(), true);
    fft->performRealOnlyForwardTransform (br.data(), true);
    if (ref != nullptr)
        fft->performRealOnlyForwardTransform (bm.data(), true);

    for (int b = 0; b < numBands; ++b)
    {
        double lr = 0, ll = 0, rr = 0, sum = 0, mm = 0;
        for (int k = edges[(size_t) b]; k < std::max (edges[(size_t) b] + 1, edges[(size_t) b + 1]); ++k)
        {
            const std::complex<double> L (bl[(size_t) (2 * k)], bl[(size_t) (2 * k + 1)]);
            const std::complex<double> R (br[(size_t) (2 * k)], br[(size_t) (2 * k + 1)]);
            const std::complex<double> M (bm[(size_t) (2 * k)], bm[(size_t) (2 * k + 1)]);
            lr += (L * std::conj (R)).real();
            ll += std::norm (L);
            rr += std::norm (R);
            sum += std::norm (0.5 * (L + R));
            mm += std::norm (M);
        }
        const double a = keep, c = 1.0 - keep;
        slr[(size_t) b] = a * slr[(size_t) b] + c * lr;
        sll[(size_t) b] = a * sll[(size_t) b] + c * ll;
        srr[(size_t) b] = a * srr[(size_t) b] + c * rr;
        ssum[(size_t) b] = a * ssum[(size_t) b] + c * sum;
        sref[(size_t) b] = a * sref[(size_t) b] + c * mm;
        const double den = std::sqrt (sll[(size_t) b] * srr[(size_t) b]);
        corr[(size_t) b] = den > 1e-20 ? (float) (slr[(size_t) b] / den) : 0.0f;
        fold[(size_t) b] = sref[(size_t) b] > 1e-20 && ssum[(size_t) b] > 1e-30
                               ? (float) (10.0 * std::log10 (ssum[(size_t) b] / sref[(size_t) b]))
                               : 0.0f;
    }
}
} // namespace sph
