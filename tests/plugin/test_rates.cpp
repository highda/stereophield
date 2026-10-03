#include <catch2/catch_test_macros.hpp>

#include "common/Render.h"
#include "common/SignalMath.h"

using namespace sph;
using namespace sph::test;

// Section 2.1 lists six sample rates; the numbered tests use three. Every
// preset must render finite, mono-safe output with the right latency at all six.
TEST_CASE ("Every preset at all six sample rates", "[rates]")
{
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        for (int preset = 1; preset <= 17; ++preset)
        {
            INFO ("fs " << fs << " preset " << preset);
            Plugin pl (fs, 512);
            pl.preset (preset);
            pl.set (ids::mid_blend, 0);
            pl.prepare();
            const int expected = (int) pl.get (ids::engine) == 1 ? (fs <= 50000 ? 2048 : fs <= 100000 ? 4096 : 8192) : 0;
            REQUIRE (pl.latency() == expected);
            const Signal x = signals::mix (fs);
            const auto y = pl.render (x);
            REQUIRE (allFinite (y.l));
            REQUIRE (allFinite (y.r));
            REQUIRE (std::max (signals::peak (y.l), signals::peak (y.r)) <= 4.0 * signals::peak (x));
            const size_t lat = (size_t) pl.latency();
            double e = 0, ref = 0;
            for (size_t i = lat; i < x.size(); ++i)
            {
                const double d = 0.5 * ((double) y.l[i] + y.r[i]) - x[i - lat];
                e += d * d;
                ref += (double) x[i] * x[i];
            }
            CHECK (10.0 * std::log10 (std::max (e, 1e-300) / ref) <= -120.0);
        }
    }
}
