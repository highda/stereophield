#include "dsp/Rng.h"

#include <cmath>
#include <numbers>

namespace sph
{
void Rng::setSeed (uint32_t seed)
{
    // Scramble the seed (murmur3 finaliser) so that neighbouring seeds such as
    // 1000 and 1001 start uncorrelated streams; xorshift alone keeps them
    // close for the first outputs. xorshift has a fixed point at zero, so
    // remap that.
    uint32_t h = seed + 0x9E3779B9u;
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    state = h != 0 ? h : 0x9E3779B9u;
    hasSpare = false;
    spare = 0.0;
}

uint32_t Rng::next()
{
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

double Rng::uniform()
{
    return static_cast<double> (next()) / 4294967296.0;
}

double Rng::gaussian()
{
    if (hasSpare)
    {
        hasSpare = false;
        return spare;
    }

    double u1 = uniform();
    while (u1 <= 0.0)
        u1 = uniform();
    const double u2 = uniform();
    const double r = std::sqrt (-2.0 * std::log (u1));
    const double a = 2.0 * std::numbers::pi * u2;
    spare = r * std::sin (a);
    hasSpare = true;
    return r * std::cos (a);
}
} // namespace sph
