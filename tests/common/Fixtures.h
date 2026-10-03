#pragma once

// Session fixtures saved by stereophield 1.0.0 (P2-T30).

#include "common/Render.h"

#include <cstdint>
#include <string>

namespace sph::test
{
// 64-bit FNV-1a over the float samples of L, then R: any bit change shows.
inline std::string hashRender (const Stereo& y)
{
    uint64_t h = 1469598103934665603ull;
    for (const auto* ch : { &y.l, &y.r })
    {
        const auto* bytes = reinterpret_cast<const unsigned char*> (ch->data());
        for (size_t i = 0; i < ch->size() * sizeof (float); ++i)
        {
            h ^= bytes[i];
            h *= 1099511628211ull;
        }
    }
    char buf[17];
    std::snprintf (buf, sizeof (buf), "%016llx", (unsigned long long) h);
    return buf;
}
} // namespace sph::test
