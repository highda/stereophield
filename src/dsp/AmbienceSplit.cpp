#include "dsp/AmbienceSplit.h"

#include <algorithm>
#include <cmath>

namespace sph
{
void AmbienceSplit::prepare (int numBins, double sampleRate, int hop)
{
    k = numBins;
    delayFrames = std::max (1, (int) std::lround (0.050 * sampleRate / hop));
    ps.assign ((size_t) ((delayFrames + 1) * k), 0.0f);
    reset();
}

void AmbienceSplit::reset()
{
    std::fill (ps.begin(), ps.end(), 0.0f);
    pos = 0;
}

void AmbienceSplit::process (const float* a, float* mt, float* mn, float* ma, double ambience, double roomDecaySeconds) noexcept
{
    const int slots = delayFrames + 1;
    const int prev = (pos + slots - 1) % slots;
    const int old = (pos + 1) % slots; // D frames before the one written now
    float* now = ps.data() + (size_t) (pos * k);
    const float* before = ps.data() + (size_t) (prev * k);
    const float* past = ps.data() + (size_t) (old * k);
    const double decay = std::exp (-0.6908 / roomDecaySeconds);

    for (int b = 0; b < k; ++b)
    {
        const double pw = (double) a[b] * a[b];
        now[b] = (float) (keep * before[b] + (1.0 - keep) * pw);
        const double pold = past[b];
        const double predicted = decay * pold;
        const double d = std::clamp (1.0 - pw / (pold + 1.0e-12), 0.0, 1.0);
        const double m = ambience * std::min (1.0, std::sqrt (predicted / (pw + 1.0e-12))) * d;
        ma[b] = (float) m;
        const double t = mt[b];
        mt[b] = (float) (t * (1.0 - m));
        mn[b] = (float) (mn[b] + t * m);
    }
    pos = (pos + 1) % slots;
}
} // namespace sph
