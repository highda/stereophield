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
        { "T27", t27Bypass },
    };
    return entries;
}
} // namespace sph::measure
