#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace sph
{
// Scope taps. Each enabled tap reduces its signal to
// min/max columns of C = fs / 400 samples, written into its own ring of
// 4096 columns (about 10 s). One writer (the audio thread), any number of
// readers (the user interface); values are relaxed atomics, the write index is
// published with release order. A disabled tap costs one flag test per block.
class ScopeTaps
{
public:
    enum Tap
    {
        inL, inR, inM, inS,
        busFull, busTonal, busNoise, busTransient,
        genSpread, genDelay, genMod, genVelvet, genPan, genCoherence, genDouble, genRoom,
        sideBus, sideSyn, envDuck, envGuard,
        outL, outR, outM, outS,
        numTaps
    };
    static constexpr int ringColumns = 4096;
    static constexpr int columnsPerSecond = 400;

    void prepare (double sampleRate);
    void reset();

    // Audio thread.
    bool enabled (int tap) const noexcept { return enable[(size_t) tap].load (std::memory_order_relaxed); }
    void write (int tap, const float* x, int numSamples) noexcept;
    // Awake mask per column (bit g for generator g, plus bits 16.. for the
    // analysis, side bus and detector), recorded with every column of outL.
    void setActivity (uint32_t mask) noexcept { activityNow = mask; }

    // Message thread.
    void setEnabled (int tap, bool on) noexcept { enable[(size_t) tap].store (on, std::memory_order_relaxed); }
    int64_t columnsWritten (int tap) const noexcept { return written[(size_t) tap].load (std::memory_order_acquire); }
    // Column c (absolute index, c < columnsWritten) of a tap.
    float columnMin (int tap, int64_t c) const noexcept { return mins[(size_t) tap][(size_t) (c % ringColumns)].load (std::memory_order_relaxed); }
    float columnMax (int tap, int64_t c) const noexcept { return maxs[(size_t) tap][(size_t) (c % ringColumns)].load (std::memory_order_relaxed); }
    uint32_t activity (int64_t c) const noexcept { return act[(size_t) (c % ringColumns)].load (std::memory_order_relaxed); }
    int samplesPerColumn() const noexcept { return columnSize; }

    static const char* tapId (int tap) noexcept;

private:
    int columnSize = 120;
    std::array<std::atomic<bool>, numTaps> enable {};
    std::array<std::vector<std::atomic<float>>, numTaps> mins, maxs;
    std::vector<std::atomic<uint32_t>> act;
    std::array<std::atomic<int64_t>, numTaps> written {};
    std::array<float, numTaps> curMin {}, curMax {};
    std::array<int, numTaps> fill {};
    uint32_t activityNow = 0;
};

// Raw output for the frequency-domain meters: (L, R, dry mid)
// triples in a single-producer single-consumer ring.
class OutputRing
{
public:
    static constexpr int capacity = 65536;
    void prepare() { data.assign ((size_t) (3 * capacity), 0.0f); reset(); }
    void reset() noexcept { writePos.store (0); readPos.store (0); }
    bool enabled() const noexcept { return on.load (std::memory_order_relaxed); }
    void setEnabled (bool v) noexcept { on.store (v, std::memory_order_relaxed); }

    void push (const float* l, const float* r, const float* m, int n) noexcept;
    // Copies up to max triples; returns how many.
    int pop (float* l, float* r, float* m, int max) noexcept;

private:
    std::vector<float> data;
    std::atomic<int64_t> writePos { 0 }, readPos { 0 };
    std::atomic<bool> on { false };
};
} // namespace sph
