#pragma once

#include "dsp/AmbienceSplit.h"
#include "dsp/ComponentSplit.h"
#include "dsp/Stft.h"

#include <complex>
#include <vector>

namespace sph
{
// Full-engine analysis (DESIGN.md section 6.2): STFT, component split and
// ambience split, producing the TONAL and NOISE buses (and, for tests, the
// transient bus), each delayed by N samples.
class Analysis
{
public:
    enum Channel { chTonal = 0, chNoise, chPan, chTransient, numChannels };

    // Which outputs this block must produce. When `stages` is false only the
    // input FIFO and hop counter run (DESIGN.md section 7.5).
    struct Needs
    {
        bool stages = true;
        bool tonal = true;
        bool noise = true;
        bool transient = false;
    };

    void prepare (double sampleRate);
    void reset();       // stage state only; the STFT keeps its framing
    void resetAll();    // stage state and the STFT

    void setParams (double ambience, double roomDecaySeconds) noexcept
    {
        amb = ambience;
        decay = roomDecaySeconds;
    }

    // Outputs may be null when not needed.
    void process (const float* m, int numSamples, const Needs& needs,
                  float* tonal, float* noise, float* transient) noexcept;

    int fftSize() const noexcept { return stft.fftSize(); }
    int hopSize() const noexcept { return stft.hopSize(); }
    int numBins() const noexcept { return stft.numBins(); }
    double sampleRate() const noexcept { return fs; }

    // Test hooks: the last analysed frame.
    int framesAnalysed() const noexcept { return frames; }
    const std::vector<float>& magnitudes() const noexcept { return mag; }
    const std::vector<float>& tonalMask() const noexcept { return mt; }
    const std::vector<float>& transientMask() const noexcept { return mx; }
    const std::vector<float>& noiseMask() const noexcept { return mn; }
    const std::vector<float>& ambienceMask() const noexcept { return ma; }

private:
    void analyseFrame (const Needs& needs) noexcept;

    double fs = 48000.0;
    double amb = 0.5, decay = 1.0;
    double magScale = 1.0;
    Stft stft;
    ComponentSplit split;
    AmbienceSplit ambience;
    std::vector<std::complex<float>> spec, bus;
    std::vector<float> mag, mt, mx, mn, ma;
    int frames = 0;
};
} // namespace sph
