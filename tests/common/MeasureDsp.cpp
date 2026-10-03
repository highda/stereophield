#include "common/MeasureDsp.h"

#include "dsp/Stft.h"
#include "signals/TestSignals.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace sph::measure
{
using namespace sph::signals;

namespace
{
// Delay in samples that maximises the cross-correlation of y with x.
int measureDelay (const Signal& x, const Signal& y, int maxLag)
{
    int best = 0;
    double bestC = -1e300;
    const size_t n = std::min (x.size(), y.size());
    for (int lag = 0; lag <= maxLag; ++lag)
    {
        double c = 0.0;
        for (size_t i = (size_t) lag; i < n; i += 7)
            c += (double) x[i - (size_t) lag] * y[i];
        if (c > bestC)
        {
            bestC = c;
            best = lag;
        }
    }
    return best;
}

// RMS of y[i] - x[i - delay] over [from, end) relative to RMS of x, in dB.
double alignedErrorDb (const Signal& x, const Signal& y, int delay, size_t from)
{
    double e = 0.0, r = 0.0;
    for (size_t i = from; i < y.size(); ++i)
    {
        const double d = (double) y[i] - (double) x[i - (size_t) delay];
        e += d * d;
        r += (double) x[i - (size_t) delay] * x[i - (size_t) delay];
    }
    return 10.0 * std::log10 (std::max (e, 1e-300) / std::max (r, 1e-300));
}
} // namespace

Result t1StftIdentity()
{
    Result r { "T1", "STFT identity: output minus input delayed by the measured latency <= -100 dB at 44.1, 48, 96 kHz", "", true, false, "" };
    double worst = -1e9;
    std::string latencies;
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        const int n = Stft::fftSizeForRate (fs);
        Stft stft;
        stft.prepare (n, n / 4, 1);
        const Signal x = noise (samples (2.0, fs));
        Signal y (x.size());
        std::vector<std::complex<float>> spec ((size_t) stft.numBins());
        for (size_t i = 0; i < x.size(); ++i)
        {
            if (stft.pushInput (x[i]))
            {
                stft.analyze (spec.data());
                stft.synthesize (0, spec.data());
            }
            y[i] = stft.popOutput (0);
        }
        const int lat = measureDelay (x, y, 2 * n);
        const double db = alignedErrorDb (x, y, lat, (size_t) (3 * n));
        worst = std::max (worst, db);
        if (lat != n)
            r.pass = false;
        latencies += (latencies.empty() ? "" : ", ") + std::to_string (lat);
    }
    r.pass = r.pass && worst <= -100.0;
    r.measured = "worst " + fmtDb (worst) + "; latency " + latencies + " samples (N)";
    return r;
}
} // namespace sph::measure
