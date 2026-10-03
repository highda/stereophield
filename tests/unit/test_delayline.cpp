#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/DelayLine.h"
#include "signals/TestSignals.h"

#include <cmath>
#include <numbers>

using Catch::Matchers::WithinAbs;

TEST_CASE ("DelayLine integer delay is exact", "[delayline]")
{
    sph::DelayLine d;
    d.prepare (1000);
    const auto x = sph::signals::noise (5000);
    for (int delay : { 0, 1, 17, 999, 1000 })
    {
        d.reset();
        for (size_t i = 0; i < x.size(); ++i)
        {
            d.push (x[i]);
            const float expected = i >= (size_t) delay ? x[i - (size_t) delay] : 0.0f;
            REQUIRE (d.readInt (delay) == expected);
            REQUIRE (d.readLinear ((double) delay) == expected);
            REQUIRE (d.readLagrange ((double) delay) == expected);
        }
    }
}

TEST_CASE ("DelayLine fractional delay of a sine has the right phase", "[delayline]")
{
    const double fs = 48000.0;
    for (double f : { 100.0, 1000.0, 5000.0 })
    {
        for (double delay : { 0.25, 3.5, 10.3, 100.77 })
        {
            sph::DelayLine d;
            d.prepare (200);
            double maxErrLin = 0.0, maxErrLag = 0.0;
            for (int i = 0; i < 4000; ++i)
            {
                d.push ((float) std::sin (2.0 * std::numbers::pi * f * i / fs));
                if (i < 300)
                    continue;
                const double expected = std::sin (2.0 * std::numbers::pi * f * (i - delay) / fs);
                maxErrLin = std::max (maxErrLin, std::abs (d.readLinear (delay) - expected));
                maxErrLag = std::max (maxErrLag, std::abs (d.readLagrange (delay) - expected));
            }
            // Interpolation error grows with frequency; at 5 kHz (0.1 of fs)
            // linear interpolation is within 1.3 % and Lagrange within 0.1 %.
            const double w = 2.0 * std::numbers::pi * f / fs;
            INFO ("f " << f << " delay " << delay << " lin " << maxErrLin << " lag " << maxErrLag);
            REQUIRE (maxErrLin <= w * w / 8.0 + 1e-6);
            REQUIRE (maxErrLag <= w * w * w * w / 24.0 + 1e-6);
        }
    }
}

TEST_CASE ("DelayLine clamps reads to the prepared length", "[delayline]")
{
    sph::DelayLine d;
    d.prepare (10);
    for (int i = 0; i < 20; ++i)
        d.push ((float) i);
    REQUIRE (d.readLinear (50.0) == d.readInt (10));
    REQUIRE (d.readLagrange (-3.0) == d.readInt (0));
}
