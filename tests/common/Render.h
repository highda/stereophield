#pragma once

// Offline driver for the full AudioProcessor (DESIGN.md section 10.1).

#include "plugin/PluginProcessor.h"
#include "plugin/Presets.h"
#include "signals/TestSignals.h"

#include <memory>

namespace sph::test
{
using signals::Signal;

struct Stereo
{
    Signal l, r;
};

class Plugin
{
public:
    explicit Plugin (double sampleRate = 48000.0, int blockSize = 512, int numInputs = 1)
        : fs (sampleRate), block (blockSize), inputs (numInputs)
    {
        proc = std::make_unique<StereophieldProcessor>();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (inputs == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        proc->setBusesLayout (layout);
        // Tests of the engine drive its parameters directly: the complete
        // interface. Easy mode tests switch back (PART3_LEDGER.md).
        set (ids::ui_mode, 1.0f);
    }

    StereophieldProcessor& processor() { return *proc; }
    Core& core() { return proc->core(); }

    // Sets a parameter in its own units (percent, Hz, ms, choice index).
    void set (const char* id, float value)
    {
        auto* p = proc->state().getParameter (id);
        jassert (p != nullptr);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    // The plain value the DSP reads.
    float get (const char* id) const
    {
        auto* p = proc->state().getParameter (id);
        if (auto* f = dynamic_cast<ExactFloatParameter*> (p))
            return f->plain();
        return p->convertFrom0to1 (p->getValue());
    }

    void setNormalised (const char* id, float value01)
    {
        proc->state().getParameter (id)->setValueNotifyingHost (value01);
    }

    // presetNumber is 1-based, as in DESIGN.md section 8.4.
    void preset (int presetNumber) { proc->setCurrentProgram (presetNumber - 1); }

    void prepare()
    {
        proc->setRateAndBufferSizeDetails (fs, block);
        proc->prepareToPlay (fs, block);
        proc->flushLatencyUpdate();
    }

    int latency() const { return proc->getLatencySamples(); }

    // Renders a mono (r == nullptr) or stereo input.
    Stereo render (const Signal& l, const Signal* r = nullptr)
    {
        Stereo out { Signal (l.size()), Signal (l.size()) };
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        for (size_t pos = 0; pos < l.size(); pos += (size_t) block)
        {
            const int n = (int) std::min ((size_t) block, l.size() - pos);
            buf.setSize (2, n, false, false, true);
            buf.clear();
            buf.copyFrom (0, 0, l.data() + pos, n);
            if (r != nullptr)
                buf.copyFrom (1, 0, r->data() + pos, n);
            proc->processBlock (buf, midi);
            std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + n, out.l.begin() + (long) pos);
            std::copy (buf.getReadPointer (1), buf.getReadPointer (1) + n, out.r.begin() + (long) pos);
        }
        return out;
    }

    double fs;
    int block;
    int inputs;

private:
    std::unique_ptr<StereophieldProcessor> proc;
};
} // namespace sph::test
