#include <catch2/catch_test_macros.hpp>

#include "common/MeasureDsp.h"

TEST_CASE ("T1 STFT identity", "[stft][T1]")
{
    const auto r = sph::measure::t1StftIdentity();
    INFO (r.measured);
    REQUIRE (r.pass);
}
