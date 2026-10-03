#include "dsp/DelayLine.h"

#include <algorithm>
#include <cmath>

namespace sph
{
void DelayLine::prepare (int maxDelay)
{
    maxDelaySamples = std::max (0, maxDelay);
    int size = 1;
    while (size < maxDelaySamples + 4)
        size <<= 1;
    buffer.assign ((size_t) size, 0.0f);
    mask = size - 1;
    reset();
}

void DelayLine::reset()
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
}

float DelayLine::readLinear (double delay) const noexcept
{
    delay = std::clamp (delay, 0.0, (double) maxDelaySamples);
    const int i = (int) delay;
    const float f = (float) (delay - i);
    const float a = readInt (i);
    const float b = readInt (i + 1);
    return a + f * (b - a);
}

float DelayLine::readLagrange (double delay) const noexcept
{
    // Third-order Lagrange over four taps at integer delays k .. k+3, with the
    // read position mu = delay - k kept in [1, 2) wherever possible.
    delay = std::clamp (delay, 0.0, (double) maxDelaySamples);
    const int k = std::max (0, (int) std::floor (delay) - 1);
    const double mu = delay - k;
    const double x0 = readInt (k), x1 = readInt (k + 1), x2 = readInt (k + 2), x3 = readInt (k + 3);
    const double m1 = mu - 1.0, m2 = mu - 2.0, m3 = mu - 3.0;
    const double h0 = -m1 * m2 * m3 / 6.0;
    const double h1 = mu * m2 * m3 / 2.0;
    const double h2 = -mu * m1 * m3 / 2.0;
    const double h3 = mu * m1 * m2 / 6.0;
    return (float) (h0 * x0 + h1 * x1 + h2 * x2 + h3 * x3);
}
} // namespace sph
