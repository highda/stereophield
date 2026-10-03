#include "common/MeasureDsp.h"

#include "common/SignalMath.h"
#include "dsp/Mod.h"
#include "dsp/Stft.h"
#include "dsp/Velvet.h"
#include "signals/TestSignals.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace sph::measure
{
using namespace sph::signals;
using sph::test::spectrum;

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

namespace sph::measure
{
namespace
{
// Runs a generator sample by sample over x, collecting its two taps.
template <typename Gen, typename PerSample>
void runTaps (Gen& gen, const Signal& x, Signal& tl, Signal& tr, PerSample&& perSample)
{
    tl.assign (x.size(), 0.0f);
    tr.assign (x.size(), 0.0f);
    float s = 0, m = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        Buses b { &x[i], &x[i], &x[i], &x[i] };
        gen.tapLeft = &tl[i];
        gen.tapRight = &tr[i];
        gen.process (b, &s, &m, 1);
        perSample (i, s, m);
    }
    gen.tapLeft = gen.tapRight = nullptr;
}

// Frequency of the strongest spectral peak, refined by parabolic
// interpolation of the log magnitude of a Hann-windowed transform.
double peakFrequency (const Signal& x, size_t from, int order, double fs)
{
    const size_t n = (size_t) 1 << order;
    Signal w (n);
    for (size_t i = 0; i < n; ++i)
        w[i] = (float) (x[from + i] * 0.5 * (1.0 - std::cos (2.0 * std::numbers::pi * (double) i / (double) n)));
    const auto X = spectrum (w.data(), n, order);
    size_t k = 1;
    for (size_t i = 1; i + 1 < X.size(); ++i)
        if (std::abs (X[i]) > std::abs (X[k]))
            k = i;
    const double a = std::log (std::abs (X[k - 1])), b = std::log (std::abs (X[k])), c = std::log (std::abs (X[k + 1]));
    const double delta = 0.5 * (a - c) / (a - 2.0 * b + c);
    return ((double) k + delta) * fs / (double) n;
}
} // namespace

Result t7ChorusAntiPhase()
{
    Result r { "T7", "Chorus: dL + dR = 2 * mod_base_ms within 0.0001 ms at every sample; no NaN", "", true, false, "" };
    const double fs = 48000.0;
    double worst = 0.0;
    bool finite = true;
    struct Case { float rate, depth, base; };
    for (const Case c : { Case { 0.4f, 1.5f, 8.0f }, Case { 5.0f, 5.0f, 8.0f }, Case { 0.05f, 2.5f, 3.0f } })
    {
        Mod mod;
        Params p;
        p.modType = ModType::Chorus;
        p.modRateHz = c.rate;
        p.modDepthMs = c.depth;
        p.modBaseMs = c.base;
        mod.prepare ({ fs, 512 });
        mod.setParams (p, true);
        const Signal x = noise (samples (10.0, fs));
        Signal tl, tr;
        runTaps (mod, x, tl, tr, [&] (size_t, float s, float m)
        {
            worst = std::max (worst, std::abs (mod.lastDelayLeftMs() + mod.lastDelayRightMs() - 2.0 * c.base));
            finite = finite && std::isfinite (s) && std::isfinite (m);
        });
    }
    r.pass = worst <= 1e-4 && finite;
    r.measured = "max |dL + dR - 2 base| " + fmt (worst * 1e6, 3) + " ns over 3 settings; " + (finite ? "no NaN" : "NaN found");
    return r;
}

Result t8MicroPitch()
{
    Result r { "T8", "Micro-pitch 9 cents on sine(1000): pL peak 1005.21 Hz, pR 994.81 Hz, each within 0.58 Hz", "", false, false, "" };
    const double fs = 48000.0;
    Mod mod;
    Params p;
    p.modType = ModType::MicroPitch;
    p.modCents = 9.0f;
    mod.prepare ({ fs, 512 });
    mod.setParams (p, true);
    const Signal x = sine (1000.0, samples (10.0, fs), fs);
    Signal tl, tr;
    runTaps (mod, x, tl, tr, [] (size_t, float, float) {});
    const size_t from = x.size() - ((size_t) 1 << 18);
    const double fl = peakFrequency (tl, from, 18, fs), fr = peakFrequency (tr, from, 18, fs);
    r.pass = std::abs (fl - 1005.21) <= 0.58 && std::abs (fr - 994.81) <= 0.58;
    r.measured = "pL " + fmt (fl, 2) + " Hz, pR " + fmt (fr, 2) + " Hz";
    return r;
}

Result t9Velvet()
{
    Result r { "T9", "Velvet on noise: |corr(vL, vR)| <= 0.25; RMS within 1 dB of input; third-octave levels 125 Hz - 16 kHz within 5 dB", "", false, true, "" };
    const double fs = 48000.0;
    Velvet v;
    Params p;
    v.prepare ({ fs, 512 });
    v.setParams (p, true);
    const Signal x = noise (samples (10.0, fs));
    Signal tl, tr;
    runTaps (v, x, tl, tr, [] (size_t, float, float) {});

    const size_t from = (size_t) samples (0.1, fs);
    const double corr = sph::test::correlation (tl, tr, from, x.size());
    const double rx = rms (x.data() + from, x.size() - from);
    const double rl = toDb (rms (tl.data() + from, x.size() - from) / rx);
    const double rr = toDb (rms (tr.data() + from, x.size() - from) / rx);

    // Third-octave levels from averaged 8192-point periodograms.
    const int order = 13;
    const size_t n = (size_t) 1 << order;
    std::vector<double> px (n / 2 + 1, 0.0), pl (n / 2 + 1, 0.0), pr (n / 2 + 1, 0.0);
    for (size_t start = from; start + n <= x.size(); start += n / 2)
    {
        for (auto [sig, acc] : { std::pair<const Signal*, std::vector<double>*> { &x, &px }, { &tl, &pl }, { &tr, &pr } })
        {
            Signal w (n);
            for (size_t i = 0; i < n; ++i)
                w[i] = (float) ((*sig)[start + i] * 0.5 * (1.0 - std::cos (2.0 * std::numbers::pi * (double) i / (double) n)));
            const auto X = spectrum (w.data(), n, order);
            for (size_t k = 0; k < X.size(); ++k)
                (*acc)[k] += std::norm (X[k]);
        }
    }
    double worstBand = 0.0;
    for (int b = -9; b <= 12; ++b) // 125 Hz .. 16 kHz
    {
        const double fc = 1000.0 * std::pow (2.0, b / 3.0);
        const size_t k0 = (size_t) std::ceil (fc * std::pow (2.0, -1.0 / 6.0) * (double) n / fs);
        const size_t k1 = (size_t) std::floor (fc * std::pow (2.0, 1.0 / 6.0) * (double) n / fs);
        double ex = 0, el = 0, er = 0;
        for (size_t k = k0; k <= k1; ++k)
        {
            ex += px[k];
            el += pl[k];
            er += pr[k];
        }
        worstBand = std::max ({ worstBand, std::abs (10.0 * std::log10 (el / ex)), std::abs (10.0 * std::log10 (er / ex)) });
    }
    r.pass = std::abs (corr) <= 0.25 && std::abs (rl) <= 1.0 && std::abs (rr) <= 1.0 && worstBand <= 5.0;
    r.measured = "corr " + fmt (corr, 3) + "; RMS L " + fmt (rl, 2) + " dB, R " + fmt (rr, 2)
                 + " dB; worst third-octave deviation " + fmt (worstBand, 2) + " dB";
    return r;
}
} // namespace sph::measure
