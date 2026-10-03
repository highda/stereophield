#pragma once

#include "dsp/Core.h"
#include "plugin/Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <vector>

namespace sph
{
class StereophieldProcessor : public juce::AudioProcessor,
                              public juce::ChangeBroadcaster,
                              private juce::AsyncUpdater
{
public:
    // Every parameter's exact plain value (PART2_LEDGER.md I7).
    using Snapshot = std::array<float, numParameters>;
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

    // Undo, redo and A/B compare (message thread). An undo point is the
    // state after a finished gesture or a preset load; restoring is exact.
    Snapshot snapshot() const;
    void restore (const Snapshot& s);
    void commitUndoPoint();
    bool undo();
    bool redo();
    bool canUndo() const noexcept { return historyIndex > 0; }
    bool canRedo() const noexcept { return historyIndex + 1 < (int) history.size(); }
    int compareSlot() const noexcept { return abSlot; }
    void selectCompareSlot (int slot);
    void copyCompareAToB();

    // User presets (I8): XML files in the user preset folder.
    static juce::File userPresetFolder();
    juce::StringArray userPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);
    bool loadStateXml (const juce::XmlElement& xml);

    // Teaching sources (L6): 0 off, 1 noise, 2 two sources, 3 melody,
    // 4 tone and click, 5 drum loop. Never saved; Off after a state load.
    void setTeachingSource (int index);
    int teachingSource() const noexcept { return teachIndex; }

    // Interface language saved with the session (0 English, 1 Czech);
    // a change is broadcast to the editor.
    int uiLanguage() const noexcept { return language; }
    void setUiLanguage (int l);

    // Applies the core's latency now; tests call this instead of running a
    // message loop.
    void flushLatencyUpdate()
    {
        cancelPendingUpdate();
        setLatencySamples (dsp.latencySamples());
    }

private:
    void handleAsyncUpdate() override;
    void completeState (juce::ValueTree& state, int version);
    void restoreExactValues (const juce::ValueTree& saved);

    juce::AudioProcessorValueTreeState apvts;
    ParamReader reader;
    Core dsp;
    int currentProgram = 0;
    std::vector<Snapshot> history;
    int historyIndex = -1;
    std::array<Snapshot, 2> abSlots {};
    bool abFilled[2] { false, false };
    int abSlot = 0;
    int language = 0;
    int teachIndex = 0;
    std::array<std::vector<float>, 6> teachSignals;
    std::atomic<const std::vector<float>*> teachActive { nullptr };
    size_t teachPos = 0;
    void setExact (juce::RangedAudioParameter* p, float plainValue);
    void buildTeachingSignals (double sampleRate);
    // True while rendering a session saved by 1.0 (see ParamReader).
    std::atomic<bool> legacyValues { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereophieldProcessor)
};
} // namespace sph
