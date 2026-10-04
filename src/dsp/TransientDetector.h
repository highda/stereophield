#pragma once

#include "dsp/OnePole.h"

namespace sph
{
// Transient envelope e(n), on the undelayed mid.
class TransientDetector
{
public:
    // Tuned constants (T15).
    static constexpr double ratioThreshold = 2.2;
    static constexpr double ratioRange = 0.6;
    static constexpr double holdSeconds = 0.100;

    void prepare (double sampleRate)
    {
        fast.setTimes (0.0001, 0.030, sampleRate);
        slow.setTimes (0.015, 0.030, sampleRate);
        hold = OnePole::coefficient (holdSeconds, sampleRate);
        reset();
    }

    void reset() noexcept
    {
        fast.reset();
        slow.reset();
        e = 0.0;
    }

    // ratio (may be null) receives fast / slow per sample, which locates an
    // onset within a frame for the spectral-flux detector.
    void process (const float* m, float* out, int numSamples, float* ratio = nullptr) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const double x = m[i] < 0.0f ? -m[i] : m[i];
            const double f = fast.process (x);
            const double s = slow.process (x);
            const double r = f / (s + 1.0e-6);
            if (ratio != nullptr)
                ratio[i] = (float) r;
            double raw = (r - ratioThreshold) / ratioRange;
            raw = f < 1.0e-4 ? 0.0 : (raw < 0.0 ? 0.0 : (raw > 1.0 ? 1.0 : raw));
            const double held = e * hold;
            e = raw > held ? raw : (held < 1.0e-30 ? 0.0 : held);
            out[i] = (float) e;
        }
    }

private:
    Follower fast, slow;
    double hold = 0.0, e = 0.0;
};
} // namespace sph
