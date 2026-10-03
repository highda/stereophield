#pragma once

#include <algorithm>
#include <cmath>

namespace sph
{
// The 20 ms fade of DESIGN.md section 5.3 for structural changes: fade the
// output to zero, apply the change and reset, then fade back in.
class ChangeFader
{
public:
    void prepare (double sampleRate) noexcept
    {
        step = 1.0f / (float) std::max (1.0, std::round (0.020 * sampleRate));
        snap();
    }

    void snap() noexcept
    {
        gain = 1.0f;
        pending = false;
    }

    // Ask for a change; the owner applies it when readyToApply() is true.
    void request() noexcept { pending = true; }
    bool isPending() const noexcept { return pending; }

    // Samples until the fade-out completes (0 when ready to apply).
    int samplesUntilSilent() const noexcept
    {
        return pending ? (int) std::ceil (gain / step - 1.0e-4f) : 0;
    }

    bool readyToApply() const noexcept { return pending && gain <= 0.0f; }
    void applied() noexcept { pending = false; }

    bool isSettled() const noexcept { return ! pending && gain >= 1.0f; }
    bool isSilent() const noexcept { return gain <= 0.0f; }

    float next() noexcept
    {
        if (pending)
            gain = std::max (0.0f, gain - step);
        else if (gain < 1.0f)
            gain = std::min (1.0f, gain + step);
        return gain;
    }

    float current() const noexcept { return gain; }

private:
    float gain = 1.0f, step = 0.001f;
    bool pending = false;
};
} // namespace sph
