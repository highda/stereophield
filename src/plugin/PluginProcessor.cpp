#include "plugin/PluginProcessor.h"

#include "plugin/Presets.h"
#include "dsp/TestSignals.h"
#include "ui/PluginEditor.h"
#include "ui/Strings.h"

namespace sph
{
StereophieldProcessor::StereophieldProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "stereophield", createParameterLayout()),
      reader (apvts)
{
    language = ui::savedLanguagePreference();
}

StereophieldProcessor::~StereophieldProcessor()
{
    cancelPendingUpdate();
}

void StereophieldProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Teaching signals are regenerated for the rate while the audio thread
    // is stopped.
    const int teach = teachIndex;
    teachActive.store (nullptr);
    buildTeachingSignals (sampleRate);
    if (teach > 0)
        teachActive.store (&teachSignals[(size_t) teach]);
    teachPos = 0;
    if (history.empty())
        commitUndoPoint();
    dsp.prepare ({ sampleRate, samplesPerBlock }, reader.read (legacyValues.load()));
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

    dsp.setParams (reader.read (legacyValues.load (std::memory_order_relaxed)));
    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);
    if (const auto* t = teachActive.load (std::memory_order_acquire); t != nullptr && ! t->empty())
    {
        // A teaching source replaces the input, looping.
        for (int i = 0; i < n; ++i)
        {
            l[i] = (*t)[teachPos];
            if (getTotalNumInputChannels() >= 2)
                r[i] = l[i];
            if (++teachPos >= t->size())
                teachPos = 0;
        }
    }
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
    commitUndoPoint();
    currentProgram = index;
    legacyValues.store (false);
    applyPreset (apvts, presets[(size_t) index]);
    dsp.requestSnap();
    commitUndoPoint();
}

StereophieldProcessor::Snapshot StereophieldProcessor::snapshot() const
{
    Snapshot s {};
    for (int i = 0; i < numParameters; ++i)
    {
        // The stepped value for choices, integers and bools, so that a
        // restore lands exactly on it.
        auto* p = apvts.getParameter (ids::all[i]);
        if (auto* f = dynamic_cast<ExactFloatParameter*> (p))
            s[(size_t) i] = f->plain();
        else if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            s[(size_t) i] = (float) c->getIndex();
        else if (auto* n = dynamic_cast<juce::AudioParameterInt*> (p))
            s[(size_t) i] = (float) n->get();
        else if (auto* b = dynamic_cast<juce::AudioParameterBool*> (p))
            s[(size_t) i] = b->get() ? 1.0f : 0.0f;
        else
            s[(size_t) i] = p->convertFrom0to1 (p->getValue());
    }
    return s;
}

void StereophieldProcessor::setExact (juce::RangedAudioParameter* p, float x)
{
    if (auto* f = dynamic_cast<ExactFloatParameter*> (p))
    {
        if (f->plain() != x)
            f->setPlain (x);
        return;
    }
    p->setValueNotifyingHost (p->convertTo0to1 (x));
}

void StereophieldProcessor::restore (const Snapshot& s)
{
    for (int i = 0; i < numParameters; ++i)
        setExact (apvts.getParameter (ids::all[i]), s[(size_t) i]);
}

void StereophieldProcessor::commitUndoPoint()
{
    const auto now = snapshot();
    if (historyIndex >= 0 && history[(size_t) historyIndex] == now)
        return;
    history.resize ((size_t) (historyIndex + 1));
    history.push_back (now);
    if (history.size() > 200)
        history.erase (history.begin());
    historyIndex = (int) history.size() - 1;
}

bool StereophieldProcessor::undo()
{
    commitUndoPoint();
    if (historyIndex <= 0)
        return false;
    restore (history[(size_t) --historyIndex]);
    // Record what the restore actually reached, so that an inexact restore
    // can never make the next undo push a new point.
    history[(size_t) historyIndex] = snapshot();
    return true;
}

bool StereophieldProcessor::redo()
{
    if (historyIndex + 1 >= (int) history.size())
        return false;
    restore (history[(size_t) ++historyIndex]);
    history[(size_t) historyIndex] = snapshot();
    return true;
}

void StereophieldProcessor::selectCompareSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == abSlot)
        return;
    abSlots[(size_t) abSlot] = snapshot();
    abFilled[abSlot] = true;
    abSlot = slot;
    if (abFilled[slot])
        restore (abSlots[(size_t) slot]);
    commitUndoPoint();
}

void StereophieldProcessor::copyCompareAToB()
{
    // The slot being edited is copied to the other one.
    abSlots[(size_t) (1 - abSlot)] = snapshot();
    abFilled[1 - abSlot] = true;
}

juce::File StereophieldProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile ("Library/Audio/Presets/highda/stereophield");
}

juce::StringArray StereophieldProcessor::userPresets() const
{
    juce::StringArray names;
    for (const auto& f : userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.sphpreset"))
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

bool StereophieldProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty())
        return false;
    juce::MemoryBlock data;
    getStateInformation (data);
    auto xml = getXmlFromBinary (data.getData(), (int) data.getSize());
    if (xml == nullptr)
        return false;
    userPresetFolder().createDirectory();
    return xml->writeTo (userPresetFolder().getChildFile (clean + ".sphpreset"));
}

bool StereophieldProcessor::loadUserPreset (const juce::String& name)
{
    const auto file = userPresetFolder().getChildFile (juce::File::createLegalFileName (name) + ".sphpreset");
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;
    commitUndoPoint();
    const bool ok = loadStateXml (*xml);
    commitUndoPoint();
    return ok;
}

bool StereophieldProcessor::deleteUserPreset (const juce::String& name)
{
    return userPresetFolder().getChildFile (juce::File::createLegalFileName (name) + ".sphpreset").deleteFile();
}

void StereophieldProcessor::buildTeachingSignals (double fs)
{
    using namespace signals;
    teachSignals[0].clear();
    teachSignals[1] = noise (samples (10.0, fs));
    teachSignals[2] = twoSource (fs);
    teachSignals[3] = melody (fs);
    teachSignals[4] = toneClick (fs);
    teachSignals[5] = drumLoop (fs);
}

void StereophieldProcessor::setTeachingSource (int index)
{
    teachIndex = juce::jlimit (0, 5, index);
    if (teachIndex > 0 && teachSignals[1].empty())
        buildTeachingSignals (getSampleRate() > 0 ? getSampleRate() : 48000.0);
    teachActive.store (teachIndex > 0 ? &teachSignals[(size_t) teachIndex] : nullptr, std::memory_order_release);
}

void StereophieldProcessor::setUiLanguage (int l)
{
    l = l == 1 ? 1 : 0;
    if (l == language)
        return;
    language = l;
    sendChangeMessage();
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
    state.setProperty ("version", 2, nullptr);
    state.setProperty ("language", language, nullptr);
    // The tree mirrors APVTS's copy of each value, which can sit a float step
    // away from the parameter's own; save the exact values.
    for (auto child : state)
        if (auto* f = dynamic_cast<ExactFloatParameter*> (apvts.getParameter (child.getProperty ("id").toString())))
            child.setProperty ("value", f->plain(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void StereophieldProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        loadStateXml (*xml);
}

bool StereophieldProcessor::loadStateXml (const juce::XmlElement& xmlIn)
{
    const auto* xml = &xmlIn;
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) state.getProperty ("program", 0);
            const int version = (int) state.getProperty ("version", 1);
            completeState (state, version);
            // replaceState writes its (possibly drifted) values back into the
            // tree, so the saved values are kept from a copy.
            const auto saved = state.createCopy();
            apvts.replaceState (state);
            if (version >= 2)
                restoreExactValues (saved);
            legacyValues.store (version < 2);
            setTeachingSource (0);
            if (state.hasProperty ("language"))
                setUiLanguage ((int) state.getProperty ("language"));
            dsp.requestSnap();
            commitUndoPoint();
            return true;
        }
    }
    return false;
}

void StereophieldProcessor::completeState (juce::ValueTree& state, int version)
{
    // A parameter missing from the saved state takes its default, or, for a
    // state saved by an older version, the value that reproduces that
    // version's sound. Without this it would keep this instance's value.
    for (const char* id : ids::all)
    {
        if (state.getChildWithProperty ("id", juce::String (id)).isValid())
            continue;
        auto* p = apvts.getParameter (id);
        float value = p->convertFrom0to1 (p->getDefaultValue());
        if (version < 2)
            legacyValue (id, value);
        juce::ValueTree child ("PARAM");
        child.setProperty ("id", juce::String (id), nullptr);
        child.setProperty ("value", value, nullptr);
        state.appendChild (child, nullptr);
    }
}

void StereophieldProcessor::restoreExactValues (const juce::ValueTree& saved)
{
    // The saved plain values, exactly (see ExactFloatParameter).
    for (const auto& child : saved)
    {
        auto* p = dynamic_cast<ExactFloatParameter*> (apvts.getParameter (child.getProperty ("id").toString()));
        if (p != nullptr && child.hasProperty ("value"))
            p->setPlain ((float) (double) child.getProperty ("value"));
    }
}

juce::AudioProcessorParameter* StereophieldProcessor::getBypassParameter() const
{
    return apvts.getParameter (ids::bypass);
}

juce::AudioProcessorEditor* StereophieldProcessor::createEditor()
{
    return new PluginEditor (*this);
}
} // namespace sph

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new sph::StereophieldProcessor();
}
