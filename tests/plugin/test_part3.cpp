#include <catch2/catch_test_macros.hpp>

#include "common/MeasurePlugin.h"

// Easy mode. Tunable criteria may pass with a documented known limit
// (docs/TESTING.md).
#define SPH_P3_TEST(fn, name, tag)                  \
    TEST_CASE (name, tag)                           \
    {                                               \
        const auto r = sph::measure::fn();          \
        INFO (r.id << ": " << r.measured);          \
        CHECK ((r.pass || ! r.note.empty()));       \
    }

SPH_P3_TEST (p3t1EasyMonoSafe, "P3-T1 Easy mode is mono-safe", "[P3-T1]")
SPH_P3_TEST (p3t2LinearWidth, "P3-T2 Width is perceptually linear", "[P3-T2]")
SPH_P3_TEST (p3t3Correlation, "P3-T3 Easy mode keeps correlation positive", "[P3-T3]")
SPH_P3_TEST (p3t4Loudness, "P3-T4 Easy mode loudness change", "[P3-T4]")
SPH_P3_TEST (p3t5BassTransients, "P3-T5 bass and transients stay centred", "[P3-T5]")
SPH_P3_TEST (p3t6ExpandInaudible, "P3-T6 expanding to Complete is inaudible", "[P3-T6]")
SPH_P3_TEST (p3t7CollapseConfirmation, "P3-T7 collapsing needs confirmation", "[P3-T7]")
SPH_P3_TEST (p3t8MacroAutomation, "P3-T8 macro automation", "[P3-T8]")
SPH_P3_TEST (p3t9Classifier, "P3-T9 material classifier", "[P3-T9]")
SPH_P3_TEST (p3t10EasyCost, "P3-T10 Easy mode cost", "[P3-T10]")
SPH_P3_TEST (p3t12SessionCompatibility, "P3-T12 sessions from 1.0 and 2.0", "[P3-T12]")
