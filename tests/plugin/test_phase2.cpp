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
