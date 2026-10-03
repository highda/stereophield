#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/OnePole.h"

#include <cmath>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE ("OnePole step response reaches 1 - 1/e after one time constant", "[onepole]")
{
    const double fs = 48000.0, tau = 0.050;
    sph::OnePole p;
    p.setTimeConstant (tau, fs);
    REQUIRE_THAT (p.getCoefficient(), WithinRel (std::exp (-1.0 / (tau * fs)), 1e-12));
    const int n = (int) std::lround (tau * fs);
    double y = 0.0;
    for (int i = 0; i < n; ++i)
        y = p.process (1.0);
    REQUIRE_THAT (y, WithinAbs (1.0 - std::exp (-1.0), 1e-4));
}

TEST_CASE ("OnePole reset sets the state", "[onepole]")
{
    sph::OnePole p;
    p.setTimeConstant (0.01, 48000.0);
    p.reset (0.5);
    REQUIRE (p.current() == 0.5);
    REQUIRE (p.process (0.5) == 0.5);
}

TEST_CASE ("Follower uses the rise coefficient above the state and fall below", "[onepole]")
{
    const double fs = 48000.0;
    sph::Follower f;
    f.setTimes (0.001, 0.100, fs);
    for (int i = 0; i < 48; ++i)
        f.process (1.0);
    REQUIRE_THAT (f.current(), WithinAbs (1.0 - std::exp (-1.0), 1e-3));
    f.reset (1.0);
    for (int i = 0; i < 4800; ++i)
        f.process (0.0);
    REQUIRE_THAT (f.current(), WithinAbs (std::exp (-1.0), 1e-3));
}
