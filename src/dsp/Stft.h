#pragma once

#include <complex>
#include <memory>
#include <vector>

namespace juce::dsp { class FFT; }

namespace sph
{
// Short-time Fourier transform with overlap-add resynthesis.
//
// Call pushInput() once per sample. When it returns true a frame of the latest
// N samples is ready: call analyze() and then synthesize() for each output
// channel that is needed. Then call popOutput() once per sample per channel.
// The output at time n is the overlap-added value for time n - N, so the
// latency is exactly N samples.
//
// The sqrt-Hann window is applied on analysis and synthesis; with hop N/4 the
// squared windows sum to 2, which the synthesis scale of 0.5 removes.
class Stft
{
public:
    Stft();
    ~Stft();

    // FFT size of DESIGN.md section 6.2.2 for a sample rate; the hop is N / 4.
    static int fftSizeForRate (double sampleRate) noexcept
    {
        return sampleRate <= 50000.0 ? 2048 : (sampleRate <= 100000.0 ? 4096 : 8192);
    }

    void prepare (int fftSize, int hopSize, int numSynthChannels);
    void reset();

    int fftSize() const noexcept { return n; }
    int hopSize() const noexcept { return hop; }
    int numBins() const noexcept { return n / 2 + 1; }
    int latency() const noexcept { return n; }
    const std::vector<float>& window() const noexcept { return win; }
    double windowSum() const noexcept { return winSum; }

    bool pushInput (float x) noexcept
    {
        input[(size_t) inPos] = x;
        inPos = (inPos + 1) & inMask;
        outPos = (outPos + 1) & accMask;
        if (++hopCount < hop)
            return false;
        hopCount = 0;
        return true;
    }

    // Windowed forward transform of the latest N inputs into numBins() bins.
    void analyze (std::complex<float>* spectrum) noexcept;

    // Inverse transform of numBins() bins, windowed and overlap-added.
    void synthesize (int channel, const std::complex<float>* spectrum) noexcept;

    float popOutput (int channel) noexcept
    {
        // outPos is the slot of the newest time n; the slot N behind it holds
        // time n - N, which no future frame touches.
        float* acc = accum[(size_t) channel].data();
        const int slot = (outPos - n) & accMask;
        const float y = acc[slot];
        acc[slot] = 0.0f;
        return y;
    }

private:
    int n = 0, hop = 0, order = 0;
    int inMask = 0, accMask = 0;
    int inPos = 0, outPos = 0, hopCount = 0;
    double winSum = 0.0;
    float synthScale = 0.5f;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> win, input, work;
    std::vector<std::vector<float>> accum;
};
} // namespace sph
