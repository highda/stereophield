#pragma once

#include "dsp/Analysis.h"
#include "dsp/Buses.h"
#include "dsp/DelayLine.h"
#include "dsp/DoubleTracker.h"
#include "dsp/HaasDelay.h"
#include "dsp/Meters.h"
#include "dsp/Mod.h"
#include "dsp/OutputStage.h"
#include "dsp/Params.h"
#include "dsp/ProcessSpec.h"
#include "dsp/RoomCues.h"
#include "dsp/ScopeTaps.h"
#include "dsp/SideBus.h"
#include "dsp/SleepController.h"
#include "dsp/Spread.h"
#include "dsp/TransientDetector.h"
#include "dsp/Velvet.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <vector>

namespace sph
{
// Two instances of a generator for seamless structural changes
// (PART2_LEDGER.md I2): the idle one takes the new structure, is pre-rolled
// on recent input and crossfades in with equal power.
template <typename G>
struct GenPair
{
    std::array<G, 2> inst;
    int cur = 0, fadePos = 1, fadeLen = 1;
    float pn = 0.0f, po = 0.0f, pc = 0.0f; // crossfade power tracking
    G& now() noexcept { return inst[(size_t) cur]; }
    const G& now() const noexcept { return inst[(size_t) cur]; }
    G& old() noexcept { return inst[(size_t) (1 - cur)]; }
    bool fading() const noexcept { return fadePos < fadeLen; }
};

// The complete signal graph of DESIGN.md section 4.1, independent of the
// plugin wrapper. Real-time safe after prepare().
class Core
{
public:
    Core();
    ~Core();

    // Allocates for the rate and block size and snaps every smoother to p.
    void prepare (const ProcessSpec& spec, const Params& p);
    void reset();

    // Call once per host block before process(). Read on the audio thread.
    void setParams (const Params& p) noexcept { params = p; }

    // The next block applies parameters without smoothing or fades
    // (DESIGN.md section 10.1, preset load).
    void requestSnap() noexcept { snapRequested.store (true); }

    // inR may be null for a mono input. Output may alias input.
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

    static int latencyFor (Engine engine, double sampleRate, LatencyMode mode = LatencyMode::PerEngine) noexcept;
    int latencySamples() const noexcept { return latency; }
    Engine activeEngine() const noexcept { return engine; }

    // Set when the engine switch changes the latency; cleared by the reader.
    std::atomic<bool> latencyChanged { false };

    // Longest tail of any awake module plus the latency, in seconds.
    double tailSeconds() const noexcept { return tail.load (std::memory_order_relaxed); }

    // Never let any module sleep (DESIGN.md section 7.6). Tests only.
    bool forceAwake = false;

    Meters meters;
    ScopeTaps scopes;     // V1, read by the interface
    OutputRing outputRing; // V6 and V7, read by the interface

    // Read-only test hooks.
    const SleepController& sleepController (GeneratorId g) const noexcept { return slots[(size_t) g].sleep; }
    const SleepController& sideBusSleep() const noexcept { return sideSleep; }
    const Spread& spread() const noexcept { return spreads.now(); }
    const HaasDelay& haas() const noexcept { return haases.now(); }
    const Mod& mod() const noexcept { return mods.now(); }
    const Velvet& velvet() const noexcept { return velvets.now(); }
    const DoubleTracker& doubler() const noexcept { return doubles.now(); }
    // True while generator g crossfades between two structures.
    bool switching (GeneratorId g) const noexcept;
    const RoomCues& room() const noexcept { return roomGen; }
    bool expanderActive() const noexcept { return expanderOn; }
    // Mean and maximum of the transient envelope e(n) over the last chunk.
    float lastEnvelopeMean() const noexcept { return envMean; }
    float lastEnvelopeMax() const noexcept { return envMax; }
    const SideBus& sideBus() const noexcept { return side; }
    const Analysis& analysis() const noexcept { return analyser; }
    const SleepController& analysisSleep() const noexcept { return analyserSleep; }
    const PanMap& panMap() const noexcept { return analyser.panMap(); }
    const CoherenceDesigner& coherence() const noexcept { return analyser.coherence(); }
    const OutputStage& output() const noexcept { return out; }
    // Generators processed in the last block, as a bit mask over GeneratorId.
    unsigned lastAwakeMask() const noexcept { return awakeMask; }

private:
    struct Slot
    {
        juce::SmoothedValue<float> amount;
        SleepController sleep;
        bool awake = true;
    };

    void applySnap (const Params& p);
    void resetAll();
    void processChunk (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;
    int samplesUntilEngineSwitch() const noexcept;

    ProcessSpec spec;
    Params params;
    std::atomic<bool> snapRequested { false };

    Engine engine = Engine::Light;
    int latency = 0;
    int fftSize = 2048;
    int engineFadeLen = 960, engineFadePos = 960;
    bool engineSwitching = false;

    DelayLine alignM, alignS, eDelay;
    Analysis analyser;
    SleepController analyserSleep;
    ChangeFader panFader;
    PanMode panMode = PanMode::Groups;
    TransientDetector detector;
    SleepController detectorSleep;
    GenPair<Spread> spreads;
    GenPair<HaasDelay> haases;
    GenPair<Mod> mods;
    GenPair<Velvet> velvets;
    GenPair<DoubleTracker> doubles;
    // Recent input of each bus, for pre-rolling a switched instance.
    std::array<std::vector<float>, 4> history;
    int historyPos = 0;
    float fadeTrack = 0.0f;
    std::vector<float> preIn, preS, preM, sOld, mOld, fullW, tapTmp, tapTmp2, guardGain, transientBus;
    template <typename G> void startSwitch (GenPair<G>& pair);
    template <typename G> void preRoll (G& g, Source src);
    RoomCues roomGen;
    juce::SmoothedValue<float> expanderMix;
    // Weight of the Full-only parts (Pan map, coherence, expander). In the
    // Always Full latency mode an engine switch fades them instead of the
    // whole output; fullWarmup delays the fade-in until the analysis is valid.
    juce::SmoothedValue<float> fullMix;
    int fullWarmup = 0;
    LatencyMode latencyMode = LatencyMode::PerEngine;
    template <typename G> void switchIfSpectral (GenPair<G>& pair, GeneratorId g);
    // Spectral-flux transient envelope (Full engine): onset markers on the
    // output time axis, the detector's ratio over the last N inputs.
    std::vector<float> fluxMarkers, ratioRing, ratio;
    int64_t inputTime = 0;
    float envMean = 0.0f, envMax = 0.0f;
    double fluxEnvelope = 0.0, fluxHold = 0.0;
    bool expanderOn = false;
    std::array<Slot, numGenerators> slots;
    SideBus side;
    SleepController sideSleep;
    OutputStage out;

    std::vector<float> m, sIn, mD, sInD, e, tonal, noise, tonalNoise, panSide, panPost, cohSide, sExp, sBus, dBus, sTmp, mTmp, sSyn, mOut, outTmpL, outTmpR;

    std::atomic<double> tail { 0.0 };
    unsigned awakeMask = 0;
};
} // namespace sph
