#pragma once

#include "common/Measure.h"

namespace sph::measure
{
Result t2MonoSafe();
Result t3Latency();
Result t4SpreadFlat();
Result t5SpreadComplementary();
Result t6Haas();
Result t13SourceGrouping();
Result t14MelodyInPlace();
Result t15TransientCentring();
Result t16Guard();
Result t27Bypass();
} // namespace sph::measure
