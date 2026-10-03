#include "common/Measure.h"

#include <cmath>
#include <cstdio>

namespace sph::measure
{
std::string fmt (double v, int decimals)
{
    char buf[64];
    std::snprintf (buf, sizeof (buf), "%.*f", decimals, v);
    return buf;
}

std::string fmtDb (double db, int decimals)
{
    if (! std::isfinite (db) || db < -300.0)
        return "-inf dB";
    return fmt (db, decimals) + " dB";
}
} // namespace sph::measure
