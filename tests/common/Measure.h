#pragma once

// Measurements Each returns the measured value and
// whether the criterion passed; Catch2 tests assert on them and sph_measure
// writes them to docs/MEASUREMENTS.md.

#include <string>
#include <vector>

namespace sph::measure
{
struct Result
{
    std::string id;
    std::string criterion;
    std::string measured;
    bool pass = false;
    bool tunable = false;
    std::string note; // fallback or remark
};

std::string fmt (double v, int decimals = 1);
std::string fmtDb (double db, int decimals = 1);

using MeasureFn = Result (*)();
} // namespace sph::measure
