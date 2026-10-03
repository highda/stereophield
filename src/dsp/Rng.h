#pragma once

#include <cstdint>

namespace sph
{
// Deterministic xorshift32 generator. Always seeded explicitly.
class Rng
{
public:
    explicit Rng (uint32_t seed = 1) { setSeed (seed); }

    void setSeed (uint32_t seed);
    uint32_t next();

    // Uniform in [0, 1).
    double uniform();

    // Standard normal deviate (Box-Muller).
    double gaussian();

private:
    uint32_t state = 1;
    bool hasSpare = false;
    double spare = 0.0;
};
} // namespace sph
