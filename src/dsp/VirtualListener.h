#pragma once

#include <array>
#include <memory>
#include <vector>

namespace juce::dsp { class FFT; }

namespace sph
{
// A listener at the apex of a +-30 degree loudspeaker triangle, modelled with
// the spherical head of Brown and Duda (1998): a head-shadow filter and an
// interaural delay per loudspeaker and ear. Computes the interaural
// cross-correlation (IACC, maximised over +-1 ms) in 16 ERB-spaced bands from
// 150 Hz to 8 kHz, and the apparent source width ASW = 1 - mean IACC of the
// 500 Hz, 1 kHz and 2 kHz bands (after IACC_E3). Frame-based on 100 ms
// windows of 4800 samples at 48 kHz. Not for the audio thread.
class VirtualListener
{
public:
    static constexpr int numBands = 16;

    VirtualListener();
    ~VirtualListener();

    // Loudspeakers at +-30 degrees (with head shadow and crosstalk), or
    // headphones (each ear hears one channel).
    enum class Playback { Speakers, Headphones };

    void prepare (double sampleRate, double smoothingSeconds = 0.3, Playback playback = Playback::Speakers);
    void reset();

    int windowSize() const noexcept { return win; }

    // Adds one window of windowSize() samples.
    void addWindow (const float* l, const float* r);

    float iacc (int band) const noexcept { return iaccs[(size_t) band]; }
    // IACC of the 500 Hz, 1 kHz and 2 kHz octave bands (IACC_E3).
    float iaccOctave (int i) const noexcept { return e3[(size_t) i]; }
    float asw() const noexcept { return width; }
    static double bandCentre (int band) noexcept;

private:
    double fs = 48000.0;
    int win = 4800, order = 13, n = 8192;
    float keep = 0.0f;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> bufL, bufR, cross;
    std::vector<std::complex<double>> earL, earR, hIpsi, hContra;
    // Bands 0..15 are ERB bands; 16..18 the octaves of IACC_E3.
    static constexpr int allBands = numBands + 3;
    std::array<int, allBands + 1> edges {};
    std::array<int, allBands> hiEdge {};
    std::array<float, numBands> iaccs {};
    std::array<float, 3> e3 {};
    std::array<std::vector<double>, allBands> sxx, syy;
    std::array<std::vector<std::complex<double>>, allBands> sxy;
    float width = 0.0f;
    bool first = true;
};
} // namespace sph
