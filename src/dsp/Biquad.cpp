#include "dsp/Biquad.h"

#include <juce_dsp/juce_dsp.h>

namespace sph
{
void Biquad::setCoefficients (const std::array<double, 6>& c) noexcept
{
    const double inv = 1.0 / c[3];
    b0 = c[0] * inv;
    b1 = c[1] * inv;
    b2 = c[2] * inv;
    a1 = c[4] * inv;
    a2 = c[5] * inv;
}

std::array<double, 6> Biquad::allPass (double sampleRate, double frequency, double q)
{
    return juce::dsp::IIR::ArrayCoefficients<double>::makeAllPass (sampleRate, frequency, q);
}

std::array<double, 6> Biquad::highPass (double sampleRate, double frequency, double q)
{
    return juce::dsp::IIR::ArrayCoefficients<double>::makeHighPass (sampleRate, frequency, q);
}

std::array<double, 6> Biquad::lowPass (double sampleRate, double frequency, double q)
{
    return juce::dsp::IIR::ArrayCoefficients<double>::makeLowPass (sampleRate, frequency, q);
}
} // namespace sph
