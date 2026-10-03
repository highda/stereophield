#include <catch2/catch_test_macros.hpp>

#include "common/MeasureDsp.h"

#define SPH_MEASURE_TEST(fn, name, tag)    \
    TEST_CASE (name, tag)                  \
    {                                      \
        const auto r = sph::measure::fn(); \
        INFO (r.id << ": " << r.measured); \
        CHECK (r.pass);                    \
    }

SPH_MEASURE_TEST (t7ChorusAntiPhase, "T7 chorus anti-phase", "[T7]")
SPH_MEASURE_TEST (t8MicroPitch, "T8 micro-pitch accuracy", "[T8]")
SPH_MEASURE_TEST (t9Velvet, "T9 velvet decorrelation", "[T9]")
