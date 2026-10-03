#pragma once

#include <cstddef>

namespace sph
{
// Integrated loudness of ITU-R BS.1770 (K-weighting, 400 ms blocks with 75 %
// overlap, absolute gate -70 LUFS, relative gate -10 LU). Offline.
struct Loudness
{
    // r may be null for one channel. Returns LUFS (-inf as -200).
    static double integrated (const float* l, const float* r, size_t numSamples, double sampleRate);
};
} // namespace sph
