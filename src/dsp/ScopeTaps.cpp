#include "dsp/ScopeTaps.h"

#include <algorithm>
#include <cmath>

namespace sph
{
const char* ScopeTaps::tapId (int tap) noexcept
{
    static const char* names[numTaps] = { "in.L", "in.R", "in.M", "in.S",
                                          "bus.full", "bus.tonal", "bus.noise", "bus.transient",
                                          "gen.spread", "gen.delay", "gen.mod", "gen.velvet", "gen.pan", "gen.coherence",
                                          "gen.double", "gen.room",
                                          "side.bus", "side.syn", "env.duck", "env.guard",
                                          "out.L", "out.R", "out.M", "out.S" };
    return tap >= 0 && tap < numTaps ? names[tap] : "";
}

void ScopeTaps::prepare (double sampleRate)
{
    columnSize = std::max (1, (int) std::lround (sampleRate / columnsPerSecond));
    for (int t = 0; t < numTaps; ++t)
    {
        mins[(size_t) t] = std::vector<std::atomic<float>> ((size_t) ringColumns);
        maxs[(size_t) t] = std::vector<std::atomic<float>> ((size_t) ringColumns);
    }
    act = std::vector<std::atomic<uint32_t>> ((size_t) ringColumns);
    reset();
}

void ScopeTaps::reset()
{
    for (int t = 0; t < numTaps; ++t)
    {
        for (auto& v : mins[(size_t) t])
            v.store (0.0f, std::memory_order_relaxed);
        for (auto& v : maxs[(size_t) t])
            v.store (0.0f, std::memory_order_relaxed);
        written[(size_t) t].store (0);
        fill[(size_t) t] = 0;
        curMin[(size_t) t] = 1e30f;
        curMax[(size_t) t] = -1e30f;
    }
    for (auto& a : act)
        a.store (0, std::memory_order_relaxed);
}

void ScopeTaps::write (int tap, const float* x, int numSamples) noexcept
{
    auto& f = fill[(size_t) tap];
    auto& lo = curMin[(size_t) tap];
    auto& hi = curMax[(size_t) tap];
    int64_t w = written[(size_t) tap].load (std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        lo = std::min (lo, x[i]);
        hi = std::max (hi, x[i]);
        if (++f >= columnSize)
        {
            const size_t at = (size_t) (w % ringColumns);
            mins[(size_t) tap][at].store (lo, std::memory_order_relaxed);
            maxs[(size_t) tap][at].store (hi, std::memory_order_relaxed);
            if (tap == outL)
                act[at].store (activityNow, std::memory_order_relaxed);
            ++w;
            written[(size_t) tap].store (w, std::memory_order_release);
            f = 0;
            lo = 1e30f;
            hi = -1e30f;
        }
    }
}

void OutputRing::push (const float* l, const float* r, const float* m, int n) noexcept
{
    int64_t w = writePos.load (std::memory_order_relaxed);
    const int64_t rd = readPos.load (std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
    {
        if (w - rd >= capacity)
            break; // reader is behind: drop
        const size_t at = (size_t) (3 * (w % capacity));
        data[at] = l[i];
        data[at + 1] = r[i];
        data[at + 2] = m[i];
        ++w;
    }
    writePos.store (w, std::memory_order_release);
}

int OutputRing::pop (float* l, float* r, float* m, int max) noexcept
{
    const int64_t w = writePos.load (std::memory_order_acquire);
    int64_t rd = readPos.load (std::memory_order_relaxed);
    int n = 0;
    while (rd < w && n < max)
    {
        const size_t at = (size_t) (3 * (rd % capacity));
        l[n] = data[at];
        r[n] = data[at + 1];
        m[n] = data[at + 2];
        ++rd;
        ++n;
    }
    readPos.store (rd, std::memory_order_release);
    return n;
}
} // namespace sph
