#include "common/MeasureDsp.h"

#include "common/SignalMath.h"
#include "dsp/Analysis.h"
#include "dsp/DoubleTracker.h"
#include "dsp/Mod.h"
#include "dsp/RoomCues.h"
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

struct VelvetStats
{
    double corr, rmsL, rmsR, worstBand;
};

VelvetStats velvetStats (VelvetDesign design, int variation);

Result t9Velvet()
{
    Result r { "T9", "Velvet (Random design) on noise: |corr(vL, vR)| <= 0.25; RMS within 1 dB of input; third-octave levels 125 Hz - 16 kHz within 5 dB", "", false, true, "" };
    const auto st = velvetStats (VelvetDesign::Random, 0);
    r.pass = std::abs (st.corr) <= 0.25 && std::abs (st.rmsL) <= 1.0 && std::abs (st.rmsR) <= 1.0 && st.worstBand <= 5.0;
    r.measured = "corr " + fmt (st.corr, 3) + "; RMS L " + fmt (st.rmsL, 2) + " dB, R " + fmt (st.rmsR, 2)
                 + " dB; worst third-octave deviation " + fmt (st.worstBand, 2) + " dB";
    return r;
}

Result p2t25OptimisedVelvet()
{
    Result r { "P2-T25", "Optimised velvet, all 16 variations: third-octave deviation <= 1.5 dB and |corr| <= 0.08", "", true, true, "" };
    double worstBand = 0.0, worstCorr = 0.0, randomBand = 0.0, randomCorr = 0.0;
    for (int v = 0; v < 16; ++v)
    {
        const auto o = velvetStats (VelvetDesign::Optimised, v);
        const auto q = velvetStats (VelvetDesign::Random, v);
        worstBand = std::max (worstBand, o.worstBand);
        worstCorr = std::max (worstCorr, std::abs (o.corr));
        randomBand = std::max (randomBand, q.worstBand);
        randomCorr = std::max (randomCorr, std::abs (q.corr));
    }
    r.pass = worstBand <= 1.5 && worstCorr <= 0.08;
    if (! r.pass && worstBand <= 2.0 && worstCorr <= 0.08)
        r.note = "known limit (docs/TESTING.md): third-octave <= 2.0 dB";
    r.measured = "worst over 16 variations: third-octave " + fmt (worstBand, 2) + " dB, |corr| " + fmt (worstCorr, 3)
                 + " (Random design: " + fmt (randomBand, 2) + " dB, " + fmt (randomCorr, 3) + ")";
    return r;
}

VelvetStats velvetStats (VelvetDesign design, int variation)
{
    const double fs = 48000.0;
    Velvet v;
    Params p;
    p.velvetDesign = design;
    p.velvetVariation = variation;
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
    return { corr, rl, rr, worstBand };
}
} // namespace sph::measure

namespace sph::measure
{
namespace
{
struct BusRender
{
    Signal tonal, noise, transient;
};

// Runs the analysis over x. perFrame(analysis, centreSeconds) is called after
// every analysed frame.
template <typename PerFrame>
BusRender runAnalysis (const Signal& x, double fs, double ambience, double decay, PerFrame&& perFrame)
{
    Analysis a;
    a.prepare (fs);
    a.setParams (ambience, decay);
    BusRender out { Signal (x.size()), Signal (x.size()), Signal (x.size()) };
    const Analysis::Needs needs { true, true, true, true };
    const int n = a.fftSize();
    for (size_t i = 0; i < x.size(); ++i)
    {
        const int before = a.framesAnalysed();
        a.process (&x[i], 1, needs, &out.tonal[i], &out.noise[i], &out.transient[i]);
        if (a.framesAnalysed() != before)
            perFrame (a, ((double) i - n / 2 + 0.5) / fs);
    }
    return out;
}

BusRender runAnalysis (const Signal& x, double fs, double ambience, double decay)
{
    return runAnalysis (x, fs, ambience, decay, [] (const Analysis&, double) {});
}

// RMS level of y relative to x over [from, end), in dB.
double levelDb (const Signal& y, const Signal& x, size_t from)
{
    return toDb (rms (y.data() + from, y.size() - from) / rms (x.data() + from, x.size() - from));
}
} // namespace

Result t10BusesSum()
{
    Result r { "T10", "Full analysis on mix: tonal + transient + noise - input delayed by Lat <= -90 dB", "", false, false, "" };
    const double fs = 48000.0;
    const Signal x = mix (fs);
    const auto b = runAnalysis (x, fs, 0.5, 1.0);
    const int lat = Stft::fftSizeForRate (fs);
    Signal sum (x.size());
    for (size_t i = 0; i < x.size(); ++i)
        sum[i] = b.tonal[i] + b.noise[i] + b.transient[i];
    const double db = sph::test::errorDb (sum, x, 1.0, lat, x, (size_t) (2 * lat), x.size());
    r.pass = db <= -90.0;
    r.measured = fmtDb (db);
    return r;
}

Result t11SplitQuality()
{
    Result r { "T11", "ambience 0. sine(440): tonal within 1 dB, others <= -20 dB. clicks: transient >= 10 dB above others. noise: noise >= 3 dB above others", "", true, true, "" };
    const double fs = 48000.0;
    const size_t from = (size_t) samples (1.0, fs);
    const int len = samples (4.0, fs);

    const Signal s = sine (440.0, len, fs);
    const auto bs = runAnalysis (s, fs, 0.0, 1.0);
    const double sT = levelDb (bs.tonal, s, from), sN = levelDb (bs.noise, s, from), sX = levelDb (bs.transient, s, from);
    const bool sineOk = std::abs (sT) <= 1.0 && sN <= -20.0 && sX <= -20.0;

    const Signal c = clicks (len, fs);
    const auto bc = runAnalysis (c, fs, 0.0, 1.0);
    const double cT = levelDb (bc.tonal, c, from), cN = levelDb (bc.noise, c, from), cX = levelDb (bc.transient, c, from);
    const bool clicksOk = cX - cT >= 10.0 && cX - cN >= 10.0;

    const Signal w = noise (len);
    const auto bw = runAnalysis (w, fs, 0.0, 1.0);
    const double wT = levelDb (bw.tonal, w, from), wN = levelDb (bw.noise, w, from), wX = levelDb (bw.transient, w, from);
    const bool noiseOk = wN - wT >= 3.0 && wN - wX >= 3.0;

    r.pass = sineOk && clicksOk && noiseOk;
    r.measured = "sine T/N/X " + fmt (sT) + "/" + fmt (sN) + "/" + fmt (sX) + " dB" + (sineOk ? "" : " (fail)")
                 + "; clicks " + fmt (cT) + "/" + fmt (cN) + "/" + fmt (cX) + " dB" + (clicksOk ? "" : " (fail)")
                 + "; noise " + fmt (wT) + "/" + fmt (wN) + "/" + fmt (wX) + " dB" + (noiseOk ? "" : " (fail)");
    return r;
}

Result t12Ambience()
{
    Result r { "T12", "decayTone, ambience 100 %, decay 1 s: magnitude-weighted mean of ma <= 0.1 over 0.3-1.0 s and >= 0.3 over 1.1-1.8 s", "", false, true, "" };
    const double fs = 48000.0;
    const Signal x = decayTone (fs);
    double steadyNum = 0, steadyDen = 0, decayNum = 0, decayDen = 0;
    runAnalysis (x, fs, 1.0, 1.0, [&] (const Analysis& a, double t)
    {
        const auto& mag = a.magnitudes();
        const auto& ma = a.ambienceMask();
        double num = 0, den = 0;
        for (size_t k = 0; k < mag.size(); ++k)
        {
            num += (double) mag[k] * ma[k];
            den += mag[k];
        }
        if (t >= 0.3 && t <= 1.0)
        {
            steadyNum += num;
            steadyDen += den;
        }
        else if (t >= 1.1 && t <= 1.8)
        {
            decayNum += num;
            decayDen += den;
        }
    });
    const double steady = steadyNum / steadyDen, dec = decayNum / decayDen;
    r.pass = steady <= 0.1 && dec >= 0.3;
    r.measured = "steady " + fmt (steady, 3) + ", decaying " + fmt (dec, 3);
    return r;
}
} // namespace sph::measure

namespace sph::measure
{
namespace
{
// Delay trajectories of both takes over a render of x.
void delayTrajectories (DoubleTracker& d, const Signal& x, std::vector<double>& a, std::vector<double>& b)
{
    a.resize (x.size());
    b.resize (x.size());
    float s = 0, m = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        Buses bus { &x[i], &x[i], &x[i], &x[i] };
        d.process (bus, &s, &m, 1);
        a[i] = d.delaySamples (0);
        b[i] = d.delaySamples (1);
    }
}
} // namespace

Result p2t19DoubleDrift()
{
    Result r { "P2-T19", "Double-tracker over 300 s: each take's delay within offset +- 3.5 drift; pitch-only wow has a standard deviation within 20 % of dbl_pitch_cents; the two takes' drifts are uncorrelated (|rho| <= 0.2)", "", false, true, "" };
    const double fs = 48000.0;
    // 300 s: a 0.3 Hz drift gives too few independent samples in 60 s to
    // estimate a correlation to +-0.2.
    const Signal x = noise (samples (300.0, fs));

    // Timing drift alone.
    DoubleTracker d;
    Params p;
    p.dblPitchCents = 0.0f;
    d.prepare ({ fs, 512 });
    d.setParams (p, true);
    std::vector<double> a, b;
    delayTrajectories (d, x, a, b);
    const double off = p.dblOffsetMs * 0.001 * fs, lim = 3.5 * p.dblDriftMs * 0.001 * fs;
    double worst = 0.0, ma = 0, mb = 0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        worst = std::max ({ worst, std::abs (a[i] - off), std::abs (b[i] - off) });
        ma += a[i];
        mb += b[i];
    }
    ma /= (double) a.size();
    mb /= (double) b.size();
    double sab = 0, saa = 0, sbb = 0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        sab += (a[i] - ma) * (b[i] - mb);
        saa += (a[i] - ma) * (a[i] - ma);
        sbb += (b[i] - mb) * (b[i] - mb);
    }
    const double rho = sab / std::sqrt (saa * sbb);

    // Pitch wow alone: cents from the delay's slope.
    DoubleTracker w;
    Params q;
    q.dblDriftMs = 0.0f;
    q.dblPitchCents = 4.0f;
    w.prepare ({ fs, 512 });
    w.setParams (q, true);
    std::vector<double> c, e;
    delayTrajectories (w, x, c, e);
    double s1 = 0, s2 = 0;
    size_t cnt = 0;
    for (size_t i = (size_t) fs; i < c.size(); ++i)
    {
        const double cents = 1200.0 * std::log2 (1.0 - (c[i] - c[i - 1]));
        s1 += cents;
        s2 += cents * cents;
        ++cnt;
    }
    const double sd = std::sqrt (s2 / (double) cnt - (s1 / (double) cnt) * (s1 / (double) cnt));
    r.pass = worst <= lim && std::abs (sd - 4.0) <= 0.8 && std::abs (rho) <= 0.2;
    r.measured = "worst delay deviation " + fmt (worst / lim * 3.5, 2) + " x drift (limit 3.5); wow " + fmt (sd, 2)
                 + " cents for 4; drift correlation " + fmt (rho, 3);
    return r;
}

Result p2t20DoubleClean()
{
    Result r { "P2-T20", "Double-tracker: at defaults no delay step above 0.05 samples; at any setting no slope change above 0.01 samples per sample (no click); no NaN under 50 random settings", "", true, false, "" };
    const double fs = 48000.0;
    const Signal x = noise (samples (10.0, fs));
    double step = 0.0, curve = 0.0;
    bool finite = true;
    for (int set = 0; set < 51; ++set)
    {
        Params p;
        if (set > 0)
        {
            Rng rng (700u + (uint32_t) set);
            p.dblOffsetMs = (float) (5.0 + 35.0 * rng.uniform());
            p.dblDriftMs = (float) (10.0 * rng.uniform());
            p.dblDriftRate = (float) (0.05 * std::pow (40.0, rng.uniform()));
            p.dblPitchCents = (float) (20.0 * rng.uniform());
            p.dblLevelDb = (float) (3.0 * rng.uniform());
            p.dblToneDb = (float) (-6.0 + 12.0 * rng.uniform());
            p.dblSeed = (int) (16 * rng.uniform());
        }
        DoubleTracker d;
        d.prepare ({ fs, 512 });
        d.setParams (p, true);
        std::vector<double> a (x.size()), b (x.size());
        float s = 0, m = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            Buses bus { &x[i], &x[i], &x[i], &x[i] };
            d.process (bus, &s, &m, 1);
            finite = finite && std::isfinite (s) && std::isfinite (m);
            a[i] = d.delaySamples (0);
            b[i] = d.delaySamples (1);
            if (i >= 2)
                for (const auto* v : { &a, &b })
                {
                    if (set == 0)
                        step = std::max (step, std::abs ((*v)[i] - (*v)[i - 1]));
                    curve = std::max (curve, std::abs ((*v)[i] - 2.0 * (*v)[i - 1] + (*v)[i - 2]));
                }
        }
    }
    r.pass = step <= 0.05 && curve <= 0.01 && finite;
    r.measured = "defaults: largest step " + fmt (step, 4) + " samples; any setting: largest slope change " + fmt (curve, 5)
                 + " samples/sample; " + (finite ? "no NaN" : "NaN found");
    return r;
}
} // namespace sph::measure

namespace sph::measure
{
Result p2t21RoomGeometry()
{
    Result r { "P2-T21", "Room cues, 8 m room, order 2, ORTF pair: every reflection's delay at each microphone within 1 sample and gain within 0.1 dB of an independent image-source calculation; none later than 80 ms; energy after 85 ms <= -100 dB; side/mid on noise >= -30 dB (the room adds width)", "", true, false, "" };
    const double fs = 48000.0, c = 343.0;
    RoomCues::Geometry g;
    g.size = 8.0f;
    g.order = 2;
    RoomCues::TapSet set;
    RoomCues::build (set, g, fs);

    // Independent enumeration: mirror the source across alternating walls.
    const double lx = g.size, ly = 0.8 * g.size, lz = 3.0;
    const double C[3] = { RoomCues::asymX * lx, RoomCues::asymY * ly, 1.2 };
    const double S[3] = { C[0], C[1] + g.distance, 1.2 }, len[3] = { lx, ly, lz };
    const double beta = std::sqrt (1.0 - g.absorb);
    struct Image { double delay[2], gain[2]; };
    std::vector<Image> expected;
    for (int ix = -2; ix <= 2; ++ix)
        for (int iy = -2; iy <= 2; ++iy)
            for (int iz = -2; iz <= 2; ++iz)
            {
                const int hits = std::abs (ix) + std::abs (iy) + std::abs (iz);
                if (hits < 1 || hits > 2)
                    continue;
                const int id[3] = { ix, iy, iz };
                double img[3];
                for (int a = 0; a < 3; ++a)
                {
                    double pos = S[a];
                    double wall = id[a] > 0 ? len[a] : 0.0;
                    for (int st = 0; st < std::abs (id[a]); ++st)
                    {
                        pos = 2.0 * wall - pos;
                        wall = wall == 0.0 ? len[a] : 0.0;
                    }
                    img[a] = pos;
                }
                const double v[3] = { img[0] - C[0], img[1] - C[1], img[2] - C[2] };
                const double rc = std::sqrt (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                if ((rc - g.distance) / c <= 0.0 || (rc - g.distance) / c > RoomCues::maxDelaySeconds)
                    continue;
                Image im {};
                for (int mic = 0; mic < 2; ++mic)
                {
                    const double mx = (mic == 0 ? -1.0 : 1.0) * RoomCues::micSpacing / 2;
                    const double dx = img[0] - (C[0] + mx);
                    const double rm = std::sqrt (dx * dx + v[1] * v[1] + v[2] * v[2]);
                    // Cardioid pointing +-55 degrees (left mic to the left),
                    // using the 3-D angle between its axis and the arrival.
                    const double ax = (mic == 0 ? -1.0 : 1.0) * std::sin (RoomCues::micAngleDeg * std::numbers::pi / 180.0);
                    const double ay = std::cos (RoomCues::micAngleDeg * std::numbers::pi / 180.0);
                    const double cosOff = (v[0] * ax + v[1] * ay) / rc;
                    im.delay[mic] = (rm - g.distance) / c * fs;
                    im.gain[mic] = (g.distance / rm) * std::pow (beta, hits) * (0.5 + 0.5 * cosOff);
                }
                expected.push_back (im);
            }

    double worstDelay = 0.0, worstGainDb = 0.0, latest = 0.0;
    for (int t = 0; t < set.count; ++t)
    {
        const auto& tap = set.taps[(size_t) t];
        const Image* best = nullptr;
        double bestD = 1e18;
        for (const auto& e : expected)
        {
            const double d = std::abs (e.delay[0] - tap.delay[0]) + std::abs (e.delay[1] - tap.delay[1])
                             + 1000.0 * (std::abs (e.gain[0] - tap.gain[0]) + std::abs (e.gain[1] - tap.gain[1]));
            if (d < bestD)
            {
                bestD = d;
                best = &e;
            }
        }
        for (int mic = 0; mic < 2; ++mic)
        {
            worstDelay = std::max (worstDelay, std::abs (best->delay[mic] - tap.delay[mic]));
            if (best->gain[mic] > 1e-6)
                worstGainDb = std::max (worstGainDb, std::abs (20.0 * std::log10 (tap.gain[mic] / best->gain[mic])));
            latest = std::max (latest, tap.delay[mic] / fs);
        }
    }

    RoomCues room;
    Params p;
    room.prepare ({ fs, 512 });
    room.setParams (p, true);
    const Signal x = impulse (samples (0.2, fs));
    double total = 0.0, late = 0.0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        float s = 0, m = 0;
        Buses b { &x[i], &x[i], &x[i], &x[i] };
        room.process (b, &s, &m, 1);
        const double e = (double) s * s + (double) m * m;
        total += e;
        if ((double) i / fs > 0.085)
            late += e;
    }
    const double lateDb = 10.0 * std::log10 (late / total + 1e-300);

    // The room must add width: side against the mid on noise.
    room.reset();
    const Signal nz = noise (samples (2.0, fs));
    double ss = 0, mm = 0;
    for (size_t i = 0; i < nz.size(); ++i)
    {
        float s = 0, m = 0;
        Buses b { &nz[i], &nz[i], &nz[i], &nz[i] };
        room.process (b, &s, &m, 1);
        ss += (double) s * s;
        mm += (double) nz[i] * nz[i];
    }
    const double sideDb = 10.0 * std::log10 (ss / mm);
    r.pass = set.count == (int) expected.size() && worstDelay <= 1.0 && worstGainDb <= 0.1 && latest <= 0.080
             && lateDb <= -100.0 && sideDb >= -30.0;
    r.measured = std::to_string (set.count) + " of " + std::to_string (expected.size()) + " reflections; worst delay error "
                 + fmt (worstDelay, 2) + " samples, gain " + fmt (worstGainDb, 3) + " dB; latest " + fmt (latest * 1000.0, 1)
                 + " ms; energy after 85 ms " + fmtDb (lateDb) + "; side/mid on noise " + fmtDb (sideDb);
    return r;
}
} // namespace sph::measure
