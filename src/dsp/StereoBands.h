#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

namespace juce::dsp { class FFT; }

namespace sph
{
// Per-band stereo measures in 24 third-octave bands from 63 Hz to 12.5 kHz:
// the correlation of L and R, and the level of the mono sum (L + R) / 2
// relative to a reference mid. Frame-based (4096 points, Hann, 50 % overlap);
// call addFrame with the latest 4096 samples every 2048 samples. Not for the
// audio thread.
class StereoBands
{
public:
    static constexpr int numBands = 24;
    static constexpr int frameSize = 4096;

    StereoBands();
    ~StereoBands();

    void prepare (double sampleRate, double smoothingSeconds = 0.2);
    void reset();

    // ref may be null (no mono-fold measure).
    void addFrame (const float* l, const float* r, const float* ref);

    static double bandCentre (int b) noexcept;
    float correlation (int b) const noexcept { return corr[(size_t) b]; }
    float monoFoldDb (int b) const noexcept { return fold[(size_t) b]; }

private:
    double fs = 48000.0;
    float keep = 0.0f;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, bl, br, bm;
    std::array<double, numBands> slr {}, sll {}, srr {}, ssum {}, sref {};
    std::array<float, numBands> corr {}, fold {};
    std::array<int, numBands + 1> edges {};
};
} // namespace sph
