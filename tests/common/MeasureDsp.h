#pragma once

#include "common/Measure.h"

namespace sph::measure
{
// STFT identity at 44.1, 48 and 96 kHz.
Result t1StftIdentity();
Result t7ChorusAntiPhase();
Result t8MicroPitch();
Result t9Velvet();
} // namespace sph::measure
