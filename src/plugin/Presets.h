#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <utility>
#include <vector>

namespace sph
{
// Factory presets Values are in parameter units:
// percent, Hz, ms, or the index of a choice.
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> changes;
};

const std::vector<Preset>& factoryPresets();

// Sets every parameter to its default, then applies the preset's changes.
void applyPreset (juce::AudioProcessorValueTreeState& state, const Preset& preset);
} // namespace sph
