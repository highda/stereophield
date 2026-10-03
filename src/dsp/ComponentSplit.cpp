#include "dsp/ComponentSplit.h"

#include <algorithm>
#include <array>

namespace sph
{
void ComponentSplit::prepare (int numBins)
{
    k = numBins;
    history.assign ((size_t) (timeMedian * k), 0.0f);
    mtPrev.assign ((size_t) k, 0.0f);
    mxPrev.assign ((size_t) k, 1.0f);
    reset();
}

void ComponentSplit::reset()
{
    std::fill (history.begin(), history.end(), 0.0f);
    std::fill (mtPrev.begin(), mtPrev.end(), 0.0f);
    std::fill (mxPrev.begin(), mxPrev.end(), 1.0f);
    histPos = 0;
}

void ComponentSplit::process (const float* a, float* mt, float* mx, float* mn) noexcept
{
    std::copy (a, a + k, history.begin() + (long) histPos * k);
    histPos = (histPos + 1) % timeMedian;

    constexpr int half = freqMedian / 2;
    std::array<float, freqMedian> fw;
    std::array<float, timeMedian> tw;
    for (int b = 0; b < k; ++b)
    {
        for (int j = 0; j < freqMedian; ++j)
            fw[(size_t) j] = a[std::clamp (b - half + j, 0, k - 1)];
        std::nth_element (fw.begin(), fw.begin() + half, fw.end());
        const double p = fw[(size_t) half];

        for (int j = 0; j < timeMedian; ++j)
            tw[(size_t) j] = history[(size_t) (j * k + b)];
        std::nth_element (tw.begin(), tw.begin() + timeMedian / 2, tw.end());
        const double h = tw[(size_t) (timeMedian / 2)];

        const double r = h / (h + p + 1.0e-12);
        const double tRaw = std::clamp ((r - tonalBreak) / breakWidth, 0.0, 1.0);
        const double xRaw = std::clamp ((transientBreak - r) / breakWidth, 0.0, 1.0);
        const double x = std::max (xRaw, 0.5 * mxPrev[(size_t) b]);
        const double t = std::min (0.5 * mtPrev[(size_t) b] + 0.5 * tRaw, 1.0 - x);
        mt[b] = (float) t;
        mx[b] = (float) x;
        mn[b] = (float) (1.0 - t - x);
        mtPrev[(size_t) b] = (float) t;
        mxPrev[(size_t) b] = (float) x;
    }
}
} // namespace sph
