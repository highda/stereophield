#pragma once

#include "dsp/Core.h"
#include "dsp/MaterialClassifier.h"
#include "plugin/EasyMode.h"
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
    // Every parameter's exact plain value.
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

    // Never let modules sleep. Tests only.
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

    // User presets: XML files in the user preset folder.
    static juce::File userPresetFolder();
    juce::StringArray userPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);
    bool loadStateXml (const juce::XmlElement& xml);

    // Teaching sources: 0 off, 1 noise, 2 two sources, 3 melody,
    // 4 tone and click, 5 drum loop. Never saved; Off after a state load.
    void setTeachingSource (int index);
    int teachingSource() const noexcept { return teachIndex; }

    // Interface language saved with the session (0 English, 1 Czech);
    // a change is broadcast to the editor.
    int uiLanguage() const noexcept { return language; }
    void setUiLanguage (int l);

    // Easy mode. Expand writes the mapped values into every
    // parameter and switches to Complete, as one undo point; nothing is
    // heard. Collapse switches back to Easy and needs the user's
    // confirmation; without it nothing changes. Message thread.
    bool isEasyMode() const;
    void expandToComplete();
    bool collapseToEasy (bool confirmed);

    // The plain values the engine would read now: the parameters, or in Easy
    // mode the mapping of the macros (with the latest classifier weights).
    Snapshot effectiveValues() const;

    // The classifier's weights (percussive, tonal, mixed), as last published
    // by the audio thread.
    easy::Weights materialWeights() const noexcept;

    // Tests and the optimiser: a candidate table instead of the shipped one
    // (null for the shipped one), and fixed material weights instead of the
    // classifier's (null to use the classifier). Call before rendering.
    void setEasyOverrides (const easy::Table* table, const easy::Weights* weights);

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
    Snapshot defaults {};
    MaterialClassifier classifier;
    std::array<std::atomic<float>, easy::numClasses> weightsOut {};
    std::vector<float> monoTmp;
    std::atomic<const easy::Table*> tableOverride { nullptr };
    std::atomic<bool> weightsForced { false };
    const easy::Table& easyTable() const noexcept
    {
        const auto* t = tableOverride.load (std::memory_order_acquire);
        return t != nullptr ? *t : easy::table();
    }
    bool stereoInput() const { return getTotalNumInputChannels() >= 2; }
    // The values for one block (audio thread).
    void blockValues (float* v);
    void buildTeachingSignals (double sampleRate);
    // True while rendering a session saved by 1.0 (see ParamReader).
    std::atomic<bool> legacyValues { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereophieldProcessor)
};
} // namespace sph
