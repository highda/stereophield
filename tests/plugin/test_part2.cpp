#include <catch2/catch_test_macros.hpp>

#include "common/MeasurePlugin.h"

#define SPH_P2_TEST(fn, name, tag)          \
    TEST_CASE (name, tag)                   \
    {                                       \
        const auto r = sph::measure::fn();  \
        INFO (r.id << ": " << r.measured);  \
        CHECK (r.pass);                     \
    }

SPH_P2_TEST (p2t14PerceivedWidth, "P2-T14 perceived width", "[P2-T14]")
SPH_P2_TEST (p2t36PerceptualMetrics, "P2-T36 perceptual metrics", "[P2-T36]")
