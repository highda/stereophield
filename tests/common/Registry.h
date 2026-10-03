#pragma once

#include "common/Measure.h"

#include <vector>

namespace sph::measure
{
struct Entry
{
    const char* id;
    MeasureFn fn;
};

// Every automated measurement, in test-id order.
const std::vector<Entry>& registry();
} // namespace sph::measure
