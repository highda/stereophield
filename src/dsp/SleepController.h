#pragma once

#include <atomic>
#include <cstdint>
#include <limits>

namespace sph
{
// Smart disable for one module (DESIGN.md section 7). update() is called once
// per block, before the module would run, with that block's input peak.
class SleepController
{
public:
    static constexpr float silenceThreshold = 1.0e-7f; // -140 dBFS

    // Freshly reset state is equivalent to an infinitely long silence.
    void reset() noexcept
    {
        silentRun = std::numeric_limits<int64_t>::max() / 4;
        asleep = false;
        fellAsleep = false;
        awakeFlag.store (true, std::memory_order_relaxed);
    }

    // Returns true if the module must run for this block. The module sleeps
    // when its gain is zero, or when its input was already silent for at
    // least requiredSilence samples before this block and is still silent,
    // so the whole of its tail has already been produced.
    bool update (bool zeroGain, float inputPeak, int numSamples, int64_t requiredSilence, bool forceAwake) noexcept
    {
        const bool silentBlock = inputPeak < silenceThreshold;
        const bool silentSleep = silentBlock && silentRun >= requiredSilence;
        silentRun = silentBlock ? silentRun + numSamples : 0;

        const bool sleepNow = ! forceAwake && (zeroGain || silentSleep);
        fellAsleep = sleepNow && ! asleep;
        asleep = sleepNow;
        awakeFlag.store (! asleep, std::memory_order_relaxed);
        return ! asleep;
    }

    // True for the block in which the module fell asleep; it must be reset once.
    bool justFellAsleep() const noexcept { return fellAsleep; }
    bool isAsleep() const noexcept { return asleep; }
    int64_t silentSamples() const noexcept { return silentRun; }

    // Read by the user interface.
    std::atomic<bool> awakeFlag { true };

private:
    int64_t silentRun = std::numeric_limits<int64_t>::max() / 4;
    bool asleep = false;
    bool fellAsleep = false;
};
} // namespace sph
