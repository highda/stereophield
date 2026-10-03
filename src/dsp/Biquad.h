#pragma once

#include <array>

namespace sph
{
// Second-order section in transposed direct form II, double precision inside.
// Coefficients come from juce::dsp::IIR::ArrayCoefficients, which computes
// the same values as IIR::Coefficients without allocating, so they can be
// recomputed on the audio thread.
class Biquad
{
public:
    // {b0, b1, b2, a0, a1, a2} as returned by ArrayCoefficients.
    void setCoefficients (const std::array<double, 6>& c) noexcept;

    static std::array<double, 6> allPass (double sampleRate, double frequency, double q);
    static std::array<double, 6> highPass (double sampleRate, double frequency, double q);
    static std::array<double, 6> lowPass (double sampleRate, double frequency, double q);

    void reset() noexcept { s1 = s2 = 0.0; }

    double process (double x) noexcept
    {
        const double y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }

private:
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double s1 = 0.0, s2 = 0.0;
};
} // namespace sph
