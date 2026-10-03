#pragma once

#include "common/Measure.h"

namespace sph::measure
{
// STFT identity at 44.1, 48 and 96 kHz.
Result t1StftIdentity();
Result t7ChorusAntiPhase();
Result t8MicroPitch();
Result t9Velvet();
Result t10BusesSum();
Result t11SplitQuality();
Result t12Ambience();
Result p2t25OptimisedVelvet();
Result p2t19DoubleDrift();
Result p2t20DoubleClean();
} // namespace sph::measure
