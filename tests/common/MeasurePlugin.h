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
Result t17SmartDisableEquivalence();
Result t18SmartDisableSaves();
Result t19NoAllocation();
Result t20NoDenormals();
Result t21Robustness();
Result t22BlockSizeInvariance();
Result t23StateRoundTrip();
Result t24AuValidation();
Result t25Performance();
Result t26PresetSanity();
Result t27Bypass();
Result t28InterfaceSnapshot();
} // namespace sph::measure
