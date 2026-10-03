#pragma once

#include <array>
#include <memory>
#include <vector>

namespace juce::dsp { class FFT; }

namespace sph
{
// Material classifier of PART3_LEDGER.md section 5. Summarises the input into
// three weights that sum to 1: percussive, tonal and mixed.
//
// A short-time spectrum (about 21 ms, hop 50 %) gives two pieces of evidence:
// - percussive onsets per second: spectral-flux peaks whose rise is
//   broadband (many more bins rise than fall, as in a hit, not a new note);
// - the tonal share of energy: energy in spectral peaks 6 dB above their
//   neighbourhood that persist from one frame to the next.
// Percussive = P (1 - T), tonal = T (1 - P), mixed = the remainder, where P
// and T map the evidence to 0 .. 1. The weights pass a 2 s one-pole, so they
// move by less than 0.05 per 100 ms. Silence holds them. Audio thread safe
// after prepare(); independent of the engine and without latency.
class MaterialClassifier
{
public:
    enum Weight { percussive = 0, tonal, mixed, numWeights };

    MaterialClassifier();
    ~MaterialClassifier();

    void prepare (double sampleRate);
    void reset();

    // Mono input.
    void process (const float* x, int numSamples) noexcept;

    float weight (int w) const noexcept { return weights[(size_t) w]; }
    const std::array<float, numWeights>& all() const noexcept { return weights; }

    // Evidence, for tests and the interface.
    float percussiveOnsetRate() const noexcept { return (float) onsetRate; }
    float tonalShare() const noexcept { return (float) tonalSmooth; }

private:
    void analyseFrame() noexcept;

    double fs = 48000.0;
    int n = 1024, hop = 512, kLo = 1, kHi = 213;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, ring, work, mag, logMag, prevLog;
    std::vector<unsigned char> peak, prevPeak;
    int ringPos = 0, hopCount = 0;
    std::array<float, 16> fluxHistory {};
    int fluxCount = 0;
    float flux1 = 0.0f, flux2 = 0.0f, broad1 = 0.0f;
    int sinceOnset = 1000;
    double onsetRate = 0.0, tonalSmooth = 0.0;
    double aRate = 0.0, aTonal = 0.0, aWeight = 0.0;
    std::array<float, numWeights> weights { 0.0f, 0.0f, 1.0f };
};
} // namespace sph
