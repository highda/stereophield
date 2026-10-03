#include "dsp/PeakScanner.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace sph
{
float PeakScanner::peak (const float* data, int numSamples) noexcept
{
    if (numSamples <= 0)
        return 0.0f;
    const auto r = juce::FloatVectorOperations::findMinAndMax (data, numSamples);
    return juce::jmax (-r.getStart(), r.getEnd());
}
} // namespace sph
