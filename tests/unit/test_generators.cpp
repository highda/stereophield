#include <catch2/catch_test_macros.hpp>

#include "common/MeasureDsp.h"

#define SPH_MEASURE_TEST(fn, name, tag)    \
    TEST_CASE (name, tag)                  \
    {                                      \
        const auto r = sph::measure::fn(); \
        INFO (r.id << ": " << r.measured); \
        CHECK ((r.pass || ! r.note.empty()));                    \
    }

SPH_MEASURE_TEST (t7ChorusAntiPhase, "T7 chorus anti-phase", "[T7]")
SPH_MEASURE_TEST (t8MicroPitch, "T8 micro-pitch accuracy", "[T8]")
SPH_MEASURE_TEST (t9Velvet, "T9 velvet decorrelation", "[T9]")
SPH_MEASURE_TEST (t10BusesSum, "T10 component buses sum to input", "[T10]")
SPH_MEASURE_TEST (t11SplitQuality, "T11 split quality", "[T11]")
SPH_MEASURE_TEST (t12Ambience, "T12 ambience split", "[T12]")
SPH_MEASURE_TEST (p2t25OptimisedVelvet, "P2-T25 optimised velvet", "[P2-T25]")
SPH_MEASURE_TEST (p2t19DoubleDrift, "P2-T19 double-tracker drift", "[P2-T19]")
SPH_MEASURE_TEST (p2t20DoubleClean, "P2-T20 double-tracker is clean", "[P2-T20]")
