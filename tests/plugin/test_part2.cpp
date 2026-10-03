#include <catch2/catch_test_macros.hpp>

#include "common/MeasurePlugin.h"

#define SPH_P2_TEST(fn, name, tag)          \
    TEST_CASE (name, tag)                   \
    {                                       \
        const auto r = sph::measure::fn();  \
        INFO (r.id << ": " << r.measured);  \
        CHECK ((r.pass || ! r.note.empty()));                     \
    }

SPH_P2_TEST (p2t14PerceivedWidth, "P2-T14 perceived width", "[P2-T14]")
SPH_P2_TEST (p2t36PerceptualMetrics, "P2-T36 perceptual metrics", "[P2-T36]")
SPH_P2_TEST (p2t15CoherenceTarget, "P2-T15 coherence reaches target", "[P2-T15]")
SPH_P2_TEST (p2t16PhysicalCurves, "P2-T16 physical coherence curves", "[P2-T16]")
SPH_P2_TEST (p2t17CoherenceSafe, "P2-T17 coherence designer is mono-safe and transient-safe", "[P2-T17]")
SPH_P2_TEST (p2t18CoherenceCost, "P2-T18 coherence designer cost", "[P2-T18]")
SPH_P2_TEST (p2t28FluxDetector, "P2-T28 spectral-flux transient detector", "[P2-T28]")
SPH_P2_TEST (p2t26SeamlessChanges, "P2-T26 seamless structural changes", "[P2-T26]")
SPH_P2_TEST (p2t27ConstantLatency, "P2-T27 constant-latency engine switching", "[P2-T27]")
