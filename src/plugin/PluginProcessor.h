#pragma once

#include "dsp/Core.h"
#include "plugin/Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace sph
{
class StereophieldProcessor : public juce::AudioProcessor,
                              private juce::AsyncUpdater
{
public:
    StereophieldProcessor();
    ~StereophieldProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "stereophield"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState& state() noexcept { return apvts; }
    Core& core() noexcept { return dsp; }
    const Core& core() const noexcept { return dsp; }

    // Never let modules sleep (DESIGN.md section 7.6). Tests only.
    void setForceAwake (bool on) noexcept { dsp.forceAwake = on; }

    // Applies the core's latency now; tests call this instead of running a
    // message loop.
    void flushLatencyUpdate()
    {
        cancelPendingUpdate();
        setLatencySamples (dsp.latencySamples());
    }

private:
    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState apvts;
    ParamReader reader;
    Core dsp;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereophieldProcessor)
};
} // namespace sph
