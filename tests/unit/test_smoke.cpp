#include <catch2/catch_test_macros.hpp>

#include "dsp/Rng.h"

TEST_CASE ("Rng is deterministic for a given seed", "[smoke]")
{
    sph::Rng a (42), b (42);
    for (int i = 0; i < 100; ++i)
        REQUIRE (a.next() == b.next());
}
