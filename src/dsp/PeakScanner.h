#pragma once

namespace sph
{
struct PeakScanner
{
    // Absolute peak of a block.
    static float peak (const float* data, int numSamples) noexcept;
};
} // namespace sph
