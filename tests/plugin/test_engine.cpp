#include <catch2/catch_test_macros.hpp>

#include "common/Render.h"
#include "common/SignalMath.h"

using namespace sph;
using namespace sph::test;
using sph::signals::samples;

TEST_CASE ("Switching engine changes the reported latency and stays clean", "[engine]")
{
    Plugin pl;
    pl.set (ids::spread_source, 1); // Tonal: uses the spectral analysis
    pl.prepare();
    REQUIRE (pl.latency() == 0);

    const Signal x = signals::mix (pl.fs);
    auto y = pl.render (x);
    REQUIRE (allFinite (y.l));

    pl.set (ids::engine, 1);
    y = pl.render (x);
    pl.processor().flushLatencyUpdate();
    REQUIRE (pl.latency() == 2048);
    REQUIRE (pl.core().activeEngine() == Engine::Full);
    REQUIRE (allFinite (y.l));
    REQUIRE (allFinite (y.r));
    REQUIRE (signals::peak (y.l) <= 4.0 * signals::peak (x));

    // The switch fades the output to zero: somewhere in the first 40 ms of
    // the render the output is silent.
    bool silent = false;
    for (int i = 0; i < samples (0.04, pl.fs); ++i)
        silent = silent || (y.l[(size_t) i] == 0.0f && y.r[(size_t) i] == 0.0f);
    REQUIRE (silent);

    // After switching, the spectral bus is live: the side is not zero.
    double side = 0;
    for (size_t i = (size_t) samples (1.5, pl.fs); i < y.l.size(); ++i)
        side += std::abs (y.l[i] - y.r[i]);
    REQUIRE (side > 1.0);

    pl.set (ids::engine, 0);
    y = pl.render (x);
    pl.processor().flushLatencyUpdate();
    REQUIRE (pl.latency() == 0);
    REQUIRE (allFinite (y.l));
}
