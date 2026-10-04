#pragma once

#include "dsp/Buses.h"
#include "dsp/ChangeFader.h"
#include "dsp/DelayLine.h"
#include "dsp/OnePole.h"
#include "dsp/ProcessSpec.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace sph
{
// Chorus and micro-pitch. Both read Lagrange-
// interpolated delays of u; s = (pL - pR) / 2, m = 0.5 ((pL + pR) / 2 - u).
//
// The chorus LFO (phase and its smoothed value) and the pitch-shifter phasors
// are not audio state: reset() leaves them running so that a module woken
// after sleeping is in phase with one that never slept.
class Mod
{
public:
    struct Structure
    {
        Source source = Source::Full;
        ModType type = ModType::Chorus;
        bool operator== (const Structure&) const = default;
    };

    void prepare (const ProcessSpec& spec);
    void reset();
    void setParams (const Params& p, bool snap);
    void process (const Buses& buses, float* s, float* m, int numSamples) noexcept;
    void advanceWhileAsleep (int numSamples) noexcept;
    // Takes over another instance's LFO and phasors (not its audio state).
    void copyPhaseFrom (const Mod& other) noexcept;
    int tailSamples() const noexcept { return (int) std::ceil (0.080 * fs); }

    const Structure& active() const noexcept { return current; }
    const Structure& pending() const noexcept { return wanted; }
    bool isSettled() const noexcept { return fader.isSettled(); }

    // When set, structural changes are left to the owner (which crossfades
    // between two instances) instead of the internal fade.
    bool externalSwitching = false;

    // Test hooks: chorus delays of the last sample in ms, and optional taps
    // that receive pL and pR.
    double lastDelayLeftMs() const noexcept { return lastDL; }
    double lastDelayRightMs() const noexcept { return lastDR; }
    float* tapLeft = nullptr;
    float* tapRight = nullptr;

private:
    void updateRates() noexcept;
    void stepLfo() noexcept;
    void stepPhasors() noexcept;

    double fs = 48000.0;
    Structure current, wanted;
    ChangeFader fader;
    DelayLine line;

    // Parameters.
    juce::SmoothedValue<float> rateHz, cents;
    OnePole baseMs, depthMs, predelayMs;
    float baseTarget = 8.0f, depthTarget = 1.5f, predelayTarget = 12.0f;

    // Chorus LFO.
    double phase = 0.0, phaseInc = 0.0;
    OnePole lfoSmooth;
    double lastDL = 0.0, lastDR = 0.0;

    // Micro-pitch phasors, left shifted up and right shifted down.
    double window = 1920.0;
    double phiL = 0.0, phiR = 0.0, phiIncL = 0.0, phiIncR = 0.0;
};
} // namespace sph
