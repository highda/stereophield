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
        { "T16", t16Guard },
        { "T27", t27Bypass },
    };
    return entries;
}
} // namespace sph::measure
