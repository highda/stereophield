#include "dsp/Meters.h"

#include <algorithm>
#include <cmath>

namespace sph
{
void Meters::prepare (double sampleRate)
{
    for (auto* p : { &lr, &ll, &rr })
        p->setTimeConstant (0.100, sampleRate);
    // At most 2048 pairs per 33 ms.
    decimation = std::max (1, (int) std::ceil (0.033 * sampleRate / 2048.0));
    fifoData.assign ((size_t) (2 * fifoPairs), 0.0f);
    reset();
}

void Meters::reset()
{
    for (auto* p : { &lr, &ll, &rr })
        p->reset();
    peaks[0].store (0.0f);
    peaks[1].store (0.0f);
    corr.store (0.0f);
    decimCount = 0;
}

void Meters::process (const float* l, const float* r, int numSamples) noexcept
{
    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        pl = std::max (pl, std::abs (l[i]));
        pr = std::max (pr, std::abs (r[i]));
        lr.process ((double) l[i] * r[i]);
        ll.process ((double) l[i] * l[i]);
        rr.process ((double) r[i] * r[i]);
    }
    if (pl > peaks[0].load (std::memory_order_relaxed))
        peaks[0].store (pl, std::memory_order_relaxed);
    if (pr > peaks[1].load (std::memory_order_relaxed))
        peaks[1].store (pr, std::memory_order_relaxed);

    const double den = std::sqrt (ll.current() * rr.current());
    corr.store (den < 1.0e-12 ? 0.0f : (float) (lr.current() / den), std::memory_order_relaxed);

    // Goniometer pairs, decimated. Pairs that do not fit are dropped.
    const int wanted = (numSamples - decimCount + decimation - 1) / decimation;
    const auto scope = fifo.write (std::max (0, wanted));
    int i = decimCount;
    auto put = [&] (int start, int size)
    {
        for (int k = 0; k < size && i < numSamples; ++k, i += decimation)
        {
            fifoData[(size_t) (2 * (start + k))] = l[i];
            fifoData[(size_t) (2 * (start + k) + 1)] = r[i];
        }
    };
    put (scope.startIndex1, scope.blockSize1);
    put (scope.startIndex2, scope.blockSize2);
    // Keep the decimation phase continuous across blocks, written or not.
    const int next = decimCount + std::max (0, wanted) * decimation;
    decimCount = next - numSamples;
}

int Meters::popGoniometer (float* dst, int maxPairs) noexcept
{
    const auto scope = fifo.read (std::min (maxPairs, fifo.getNumReady()));
    int n = 0;
    for (int k = 0; k < scope.blockSize1; ++k, ++n)
    {
        dst[2 * n] = fifoData[(size_t) (2 * (scope.startIndex1 + k))];
        dst[2 * n + 1] = fifoData[(size_t) (2 * (scope.startIndex1 + k) + 1)];
    }
    for (int k = 0; k < scope.blockSize2; ++k, ++n)
    {
        dst[2 * n] = fifoData[(size_t) (2 * (scope.startIndex2 + k))];
        dst[2 * n + 1] = fifoData[(size_t) (2 * (scope.startIndex2 + k) + 1)];
    }
    return n;
}
} // namespace sph
