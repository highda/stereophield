#include <catch2/catch_test_macros.hpp>

#include "common/MeasurePlugin.h"

#define SPH_MEASURE_TEST(fn, name, tag)      \
    TEST_CASE (name, tag)                    \
    {                                        \
        const auto r = sph::measure::fn();   \
        INFO (r.id << ": " << r.measured);   \
        UNSCOPED_INFO (r.measured);          \
        CHECK (r.pass);                      \
    }

SPH_MEASURE_TEST (t2MonoSafe, "T2 mono-safe invariant", "[T2]")
SPH_MEASURE_TEST (t3Latency, "T3 reported latency", "[T3]")
SPH_MEASURE_TEST (t4SpreadFlat, "T4 spread power is flat", "[T4]")
SPH_MEASURE_TEST (t5SpreadComplementary, "T5 spread is complementary", "[T5]")
SPH_MEASURE_TEST (t6Haas, "T6 authentic Haas", "[T6]")
SPH_MEASURE_TEST (t16Guard, "T16 correlation guard", "[T16]")
SPH_MEASURE_TEST (t27Bypass, "T27 bypass alignment", "[T27]")
SPH_MEASURE_TEST (t15TransientCentring, "T15 transient centring", "[T15]")
SPH_MEASURE_TEST (t13SourceGrouping, "T13 source grouping", "[T13]")
SPH_MEASURE_TEST (t14MelodyInPlace, "T14 melody stays in place", "[T14]")
SPH_MEASURE_TEST (t17SmartDisableEquivalence, "T17 smart disable changes nothing", "[T17]")
SPH_MEASURE_TEST (t18SmartDisableSaves, "T18 smart disable saves work", "[T18]")
SPH_MEASURE_TEST (t19NoAllocation, "T19 no allocation on the audio thread", "[T19]")
SPH_MEASURE_TEST (t20NoDenormals, "T20 no denormals", "[T20]")
SPH_MEASURE_TEST (t21Robustness, "T21 robustness", "[T21]")
SPH_MEASURE_TEST (t22BlockSizeInvariance, "T22 block-size invariance", "[T22]")
SPH_MEASURE_TEST (t23StateRoundTrip, "T23 state round trip", "[T23]")
SPH_MEASURE_TEST (t24AuValidation, "T24 Audio Unit validation", "[T24]")
SPH_MEASURE_TEST (t25Performance, "T25 performance", "[T25]")
SPH_MEASURE_TEST (t26PresetSanity, "T26 preset sanity", "[T26]")
