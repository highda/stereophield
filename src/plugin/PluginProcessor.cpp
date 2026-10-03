#include "plugin/PluginProcessor.h"

#include "plugin/Presets.h"

namespace sph
{
StereophieldProcessor::StereophieldProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "stereophield", createParameterLayout()),
      reader (apvts)
{
}

StereophieldProcessor::~StereophieldProcessor()
{
    cancelPendingUpdate();
}

void StereophieldProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp.prepare ({ sampleRate, samplesPerBlock }, reader.read());
    dsp.latencyChanged.store (false);
    setLatencySamples (dsp.latencySamples());
}

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
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    dsp.setParams (reader.read());
    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);
    dsp.process (l, getTotalNumInputChannels() >= 2 ? r : nullptr, l, r, n);

    if (dsp.latencyChanged.exchange (false))
        triggerAsyncUpdate();
}

void StereophieldProcessor::handleAsyncUpdate()
{
    setLatencySamples (dsp.latencySamples());
}

double StereophieldProcessor::getTailLengthSeconds() const
{
    return dsp.tailSeconds();
}

int StereophieldProcessor::getNumPrograms()
{
    return (int) factoryPresets().size();
}

void StereophieldProcessor::setCurrentProgram (int index)
{
    const auto& presets = factoryPresets();
    if (index < 0 || index >= (int) presets.size())
        return;
    currentProgram = index;
    applyPreset (apvts, presets[(size_t) index]);
    dsp.requestSnap();
}

const juce::String StereophieldProcessor::getProgramName (int index)
{
    const auto& presets = factoryPresets();
    return index >= 0 && index < (int) presets.size() ? presets[(size_t) index].name : "";
}

void StereophieldProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void StereophieldProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) state.getProperty ("program", 0);
            apvts.replaceState (state);
            dsp.requestSnap();
        }
}

juce::AudioProcessorParameter* StereophieldProcessor::getBypassParameter() const
{
    return apvts.getParameter (ids::bypass);
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
