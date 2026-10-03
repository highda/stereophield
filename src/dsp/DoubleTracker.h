#pragma once

#include "dsp/Biquad.h"
#include "dsp/Buses.h"
#include "dsp/ChangeFader.h"
#include "dsp/DelayLine.h"
#include "dsp/OnePole.h"
#include "dsp/ProcessSpec.h"
#include "dsp/Rng.h"

#include <array>

namespace sph
{
// Humanised artificial double tracking (PART2_LEDGER.md G7). Two synthetic
// takes A (left) and B (right) read the bus at slowly and independently
// drifting delays, with a faster random "wow" for pitch, a level drift and a
// tone difference. s = (A - B) / 2, m = (A + B) / 2 - u.
class DoubleTracker
{
public:
    static constexpr int controlInterval = 32;

    struct Structure
    {
        Source source = Source::Full;
        int seed = 0;
        bool operator== (const Structure&) const = default;
    };

    void prepare (const ProcessSpec& spec);
    void reset();
    void setParams (const Params& p, bool snap);
    void process (const Buses& buses, float* s, float* m, int numSamples) noexcept;
    void advanceWhileAsleep (int numSamples) noexcept;
    // Takes over another instance's drift processes (same seed only).
    void copyDriftFrom (const DoubleTracker& other) noexcept;
    int tailSamples() const noexcept { return (int) std::ceil (0.065 * fs); }

    const Structure& active() const noexcept { return current; }
    const Structure& pending() const noexcept { return wanted; }
    bool isSettled() const noexcept { return fader.isSettled(); }

    // When set, structural changes are left to the owner (which crossfades
    // between two instances) instead of the internal fade.
    bool externalSwitching = false;

    // Test hooks: read delay of a take in samples at the last sample, and
    // optional taps receiving the two takes.
    double delaySamples (int take) const noexcept { return lastDelay[(size_t) take]; }
    float* tapA = nullptr;
    float* tapB = nullptr;

private:
    // Gaussian noise through two cascaded one-poles, normalised to unit
    // standard deviation, stepped at the control rate.
    struct Drift
    {
        double y1 = 0, y2 = 0, a = 0, norm = 1;
        void setRate (double hz, double controlRate) noexcept;
        double step (Rng& rng) noexcept
        {
            const double x = rng.gaussian();
            y1 = a * y1 + (1.0 - a) * x;
            y2 = a * y2 + (1.0 - a) * y1;
            return y2 * norm;
        }
    };

    struct Take
    {
        Rng rng;
        Drift timing, wow, level;
        double prevDelay = 0, nextDelay = 0, prevGain = 1, nextGain = 1;
        Biquad shelf;
        OnePole delaySmooth; // keeps the delay's slope (pitch) continuous
    };

    void controlStep() noexcept;
    void seedTakes() noexcept;
    void updateRates() noexcept;

    double fs = 48000.0, controlRate = 1500.0;
    Structure current, wanted;
    ChangeFader fader;
    DelayLine line;
    std::array<Take, 2> takes;
    int phase = 0;
    OnePole offsetMs;
    float offsetTarget = 18.0f, driftMs = 3.0f, driftRate = 0.3f, pitchCents = 4.0f, levelDb = 0.7f, toneDb = -1.5f;
    float appliedTone = 1e9f, appliedRate = -1.0f;
    double wowAmplitudeSeconds = 0.0, wowSlope = 1.0;
    std::array<double, 2> lastDelay {};
};
} // namespace sph
