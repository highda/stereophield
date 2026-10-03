#pragma once

#include "dsp/Analysis.h"
#include "dsp/Buses.h"
#include "dsp/DelayLine.h"
#include "dsp/HaasDelay.h"
#include "dsp/Meters.h"
#include "dsp/Mod.h"
#include "dsp/OutputStage.h"
#include "dsp/Params.h"
#include "dsp/ProcessSpec.h"
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

    static int latencyFor (Engine engine, double sampleRate) noexcept;
    int latencySamples() const noexcept { return latency; }
    Engine activeEngine() const noexcept { return engine; }

    // Set when the engine switch changes the latency; cleared by the reader.
    std::atomic<bool> latencyChanged { false };

    // Longest tail of any awake module plus the latency, in seconds.
    double tailSeconds() const noexcept { return tail.load (std::memory_order_relaxed); }

    // Never let any module sleep (DESIGN.md section 7.6). Tests only.
    bool forceAwake = false;

    Meters meters;

    // Read-only test hooks.
    const SleepController& sleepController (GeneratorId g) const noexcept { return slots[(size_t) g].sleep; }
    const SleepController& sideBusSleep() const noexcept { return sideSleep; }
    const Spread& spread() const noexcept { return spreadGen; }
    const HaasDelay& haas() const noexcept { return haasGen; }
    const Mod& mod() const noexcept { return modGen; }
    const Velvet& velvet() const noexcept { return velvetGen; }
    const SideBus& sideBus() const noexcept { return side; }
    const Analysis& analysis() const noexcept { return analyser; }
    const SleepController& analysisSleep() const noexcept { return analyserSleep; }
    const PanMap& panMap() const noexcept { return analyser.panMap(); }
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
    Spread spreadGen;
    HaasDelay haasGen;
    Mod modGen;
    Velvet velvetGen;
    std::array<Slot, numGenerators> slots;
    SideBus side;
    SleepController sideSleep;
    OutputStage out;

    std::vector<float> m, sIn, mD, sInD, e, tonal, noise, tonalNoise, panSide, panPost, sBus, dBus, sTmp, mTmp, sSyn, mOut, outTmpL, outTmpR;

    std::atomic<double> tail { 0.0 };
    unsigned awakeMask = 0;
};
} // namespace sph
