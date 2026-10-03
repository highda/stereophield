#include "plugin/PluginProcessor.h"

namespace sph
{
StereophieldProcessor::StereophieldProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void StereophieldProcessor::prepareToPlay (double, int) {}

bool StereophieldProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo())
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void StereophieldProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int n = buffer.getNumSamples();

    // Mono input arrives on channel 0 only; copy it to the right channel.
    if (numIn == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
}

juce::AudioProcessorEditor* StereophieldProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
} // namespace sph

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new sph::StereophieldProcessor();
}
