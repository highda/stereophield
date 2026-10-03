#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/Biquad.h"
#include "dsp/PeakScanner.h"

#include <cmath>
#include <complex>
#include <numbers>

using Catch::Matchers::WithinAbs;

namespace
{
std::complex<double> response (const std::array<double, 6>& c, double f, double fs)
{
    const auto z1 = std::polar (1.0, -2.0 * std::numbers::pi * f / fs);
    const auto z2 = z1 * z1;
    return (c[0] + c[1] * z1 + c[2] * z2) / (c[3] + c[4] * z1 + c[5] * z2);
}
} // namespace

TEST_CASE ("Biquad all-pass has unit magnitude and filters like its coefficients", "[biquad]")
{
    const double fs = 48000.0;
    const auto c = sph::Biquad::allPass (fs, 1000.0, 0.7);
    for (double f : { 20.0, 500.0, 1000.0, 5000.0, 20000.0 })
        REQUIRE_THAT (std::abs (response (c, f, fs)), WithinAbs (1.0, 1e-12));

    // A sine through the filter matches the analytic response.
    sph::Biquad b;
    b.setCoefficients (c);
    const double f = 1500.0;
    const auto h = response (c, f, fs);
    double maxErr = 0.0;
    for (int i = 0; i < 20000; ++i)
    {
        const double y = b.process (std::sin (2.0 * std::numbers::pi * f * i / fs));
        if (i > 5000)
            maxErr = std::max (maxErr, std::abs (y - std::abs (h) * std::sin (2.0 * std::numbers::pi * f * i / fs + std::arg (h))));
    }
    REQUIRE (maxErr < 1e-9);
}

TEST_CASE ("Biquad high-pass and low-pass have the expected corner", "[biquad]")
{
    const double fs = 48000.0, q = 1.0 / std::sqrt (2.0);
    REQUIRE_THAT (std::abs (response (sph::Biquad::highPass (fs, 150.0, q), 150.0, fs)), WithinAbs (std::sqrt (0.5), 1e-6));
    REQUIRE_THAT (std::abs (response (sph::Biquad::lowPass (fs, 150.0, q), 150.0, fs)), WithinAbs (std::sqrt (0.5), 1e-6));
    REQUIRE (std::abs (response (sph::Biquad::highPass (fs, 150.0, q), 15.0, fs)) < 0.011);
}

TEST_CASE ("PeakScanner returns the absolute peak", "[peakscanner]")
{
    const float a[] = { 0.1f, -0.7f, 0.3f, 0.5f };
    REQUIRE (sph::PeakScanner::peak (a, 4) == 0.7f);
    const float b[] = { 0.1f, 0.2f, -0.05f };
    REQUIRE (sph::PeakScanner::peak (b, 3) == 0.2f);
    REQUIRE (sph::PeakScanner::peak (a, 0) == 0.0f);
}
