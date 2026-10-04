#pragma once

#include "dsp/Buses.h"
#include "dsp/ChangeFader.h"
#include "dsp/DelayLine.h"
#include "dsp/ProcessSpec.h"

#include <array>

namespace sph
{
// Velvet-noise decorrelator.
//   s = (vL - vR) / 2,  m = (vL + vR) / 2 - u.
class Velvet
{
public:
    static constexpr int maxImpulses = 240;

    struct Structure
    {
        Source source = Source::Full;
        float sizeMs = 30.0f;
        float density = 1000.0f;
        int variation = 0;
        VelvetDesign design = VelvetDesign::Optimised;
        bool operator== (const Structure&) const = default;
    };

    struct Sequence
    {
        std::array<int, maxImpulses> pos {};
        std::array<float, maxImpulses> gain {};
        int count = 0;
    };

    // Builds one sequence; allocation-free, so it may run on the audio thread.
    static void build (Sequence& seq, double sampleRate, float sizeMs, float density, uint32_t seed) noexcept;

    // Optimised sequence for a variation and side: the
    // table class nearest the specified impulse count, scaled to the length.
    static void buildOptimised (Sequence& seq, double sampleRate, float sizeMs, float density, int variation, int side) noexcept;

    void prepare (const ProcessSpec& spec);
    void reset();
    void setParams (const Params& p, bool snap);
    void process (const Buses& buses, float* s, float* m, int numSamples) noexcept;
    int tailSamples() const noexcept { return length; }

    const Structure& active() const noexcept { return current; }
    const Structure& pending() const noexcept { return wanted; }
    bool isSettled() const noexcept { return fader.isSettled(); }

    // When set, structural changes are left to the owner (which crossfades
    // between two instances) instead of the internal fade.
    bool externalSwitching = false;

    // Test hooks.
    const Sequence& left() const noexcept { return seqL; }
    const Sequence& right() const noexcept { return seqR; }
    float* tapLeft = nullptr;
    float* tapRight = nullptr;

private:
    void rebuild() noexcept;

    double fs = 48000.0;
    Structure current, wanted;
    ChangeFader fader;
    DelayLine line;
    Sequence seqL, seqR;
    int length = 0;
};
} // namespace sph
