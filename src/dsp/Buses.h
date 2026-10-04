#pragma once

#include "dsp/Params.h"

namespace sph
{
// The component signals a generator can read. In the
// Light engine every pointer is the input mid.
struct Buses
{
    const float* full = nullptr;
    const float* tonal = nullptr;
    const float* noise = nullptr;
    const float* tonalNoise = nullptr;

    const float* get (Source s) const noexcept
    {
        switch (s)
        {
            case Source::Tonal: return tonal;
            case Source::Noise: return noise;
            case Source::TonalNoise: return tonalNoise;
            case Source::Full: break;
        }
        return full;
    }
};

inline bool isSpectral (Source s) noexcept { return s != Source::Full; }
inline bool usesTonal (Source s) noexcept { return s == Source::Tonal || s == Source::TonalNoise; }
inline bool usesNoise (Source s) noexcept { return s == Source::Noise || s == Source::TonalNoise; }
} // namespace sph
