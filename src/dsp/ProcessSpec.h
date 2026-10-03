#pragma once

namespace sph
{
struct ProcessSpec
{
    double sampleRate = 48000.0;
    int maxBlockSize = 512;
};
} // namespace sph
