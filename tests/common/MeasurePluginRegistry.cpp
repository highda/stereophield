#include "common/Registry.h"

#include "common/MeasureDsp.h"
#include "common/MeasurePlugin.h"

namespace sph::measure
{
const std::vector<Entry>& registry()
{
    static const std::vector<Entry> entries {
        { "T1", t1StftIdentity },
        { "T2", t2MonoSafe },
        { "T3", t3Latency },
        { "T4", t4SpreadFlat },
        { "T5", t5SpreadComplementary },
        { "T6", t6Haas },
        { "T7", t7ChorusAntiPhase },
        { "T8", t8MicroPitch },
        { "T9", t9Velvet },
        { "T10", t10BusesSum },
        { "T11", t11SplitQuality },
        { "T12", t12Ambience },
        { "T13", t13SourceGrouping },
        { "T14", t14MelodyInPlace },
        { "T15", t15TransientCentring },
        { "T16", t16Guard },
        { "T17", t17SmartDisableEquivalence },
        { "T18", t18SmartDisableSaves },
        { "T19", t19NoAllocation },
        { "T20", t20NoDenormals },
        { "T21", t21Robustness },
        { "T22", t22BlockSizeInvariance },
        { "T23", t23StateRoundTrip },
        { "T24", t24AuValidation },
        { "T25", t25Performance },
        { "T26", t26PresetSanity },
        { "T27", t27Bypass },
        { "T28", t28InterfaceSnapshot },
        { "P2-T14", p2t14PerceivedWidth },
        { "P2-T15", p2t15CoherenceTarget },
        { "P2-T16", p2t16PhysicalCurves },
        { "P2-T17", p2t17CoherenceSafe },
        { "P2-T18", p2t18CoherenceCost },
        { "P2-T25", p2t25OptimisedVelvet },
        { "P2-T36", p2t36PerceptualMetrics },
    };
    return entries;
}
} // namespace sph::measure
