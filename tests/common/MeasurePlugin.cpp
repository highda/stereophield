#include "common/MeasurePlugin.h"

#include "common/Render.h"
#include "common/SignalMath.h"

#include <algorithm>
#include <cmath>

namespace sph::measure
{
using namespace sph::test;
using namespace sph::signals;

namespace
{
constexpr double rates[] = { 44100.0, 48000.0, 96000.0 };

// Worst mono-safe error of one prepared plugin on the mix signal, in dB.
double monoSafeErrorDb (Plugin& pl)
{
    const Signal x = mix (pl.fs);
    const auto y = pl.render (x);
    const double g = std::pow (10.0, pl.get (ids::out_gain_db) / 20.0);
    const size_t lat = (size_t) pl.latency();
    double e = 0.0, ref = 0.0;
    for (size_t i = lat; i < x.size(); ++i)
    {
        const double d = 0.5 * ((double) y.l[i] + (double) y.r[i]) - g * x[i - lat];
        e += d * d;
        ref += (double) x[i] * x[i];
    }
    return 10.0 * std::log10 (std::max (e, 1e-300) / ref);
}

void forceMonoSafe (Plugin& pl)
{
    pl.set (ids::mid_blend, 0);
    pl.set (ids::comp_mode, 0);
    pl.set (ids::bypass, 0);
    pl.set (ids::listen, 0);
}

void randomise (Plugin& pl, uint32_t seed)
{
    Rng rng (seed);
    for (const char* id : ids::all)
        pl.setNormalised (id, (float) rng.uniform());
}

void spreadOnly (Plugin& pl, int spreadType)
{
    pl.set (ids::spread_amount, 100);
    pl.set (ids::spread_type, (float) spreadType);
    pl.set (ids::bass_mono_hz, 20);
    pl.set (ids::transient_duck, 0);
    pl.set (ids::guard, 0);
}

// |L|^2 and |R|^2 of the impulse response of the spread-only setup.
void spreadResponse (int spreadType, std::vector<double>& pl2, std::vector<double>& pr2, double& binHz)
{
    Plugin pl;
    spreadOnly (pl, spreadType);
    pl.prepare();
    const auto y = pl.render (impulse (65536));
    const auto L = spectrum (y.l.data(), y.l.size(), 16);
    const auto R = spectrum (y.r.data(), y.r.size(), 16);
    pl2.resize (L.size());
    pr2.resize (L.size());
    // Normalise by the input impulse amplitude.
    for (size_t k = 0; k < L.size(); ++k)
    {
        pl2[k] = std::norm (L[k]) / (0.25 * 0.25);
        pr2[k] = std::norm (R[k]) / (0.25 * 0.25);
    }
    binHz = pl.fs / 65536.0;
}
} // namespace

Result t2MonoSafe()
{
    Result r { "T2", "Mono-safe: (L+R)/2 - out_gain * M_d <= -120 dB (every preset with mid_blend 0, 20 random sets; mix; 44.1, 48, 96 kHz)", "", false, false, "" };
    double worst = -1e9;
    int renders = 0;
    for (double fs : rates)
    {
        for (int preset = 1; preset <= (int) factoryPresets().size(); ++preset)
        {
            Plugin pl (fs);
            pl.preset (preset);
            pl.set (ids::mid_blend, 0);
            pl.prepare();
            worst = std::max (worst, monoSafeErrorDb (pl));
            ++renders;
        }
        for (uint32_t set = 0; set < 20; ++set)
        {
            Plugin pl (fs);
            randomise (pl, 1000 + set);
            forceMonoSafe (pl);
            pl.prepare();
            worst = std::max (worst, monoSafeErrorDb (pl));
            ++renders;
        }
    }
    r.pass = worst <= -120.0;
    r.measured = "worst " + fmtDb (worst) + " over " + std::to_string (renders) + " renders";
    return r;
}

Result t3Latency()
{
    Result r { "T3", "Peak of (L+R)/2 for an impulse with width 0 is at getLatencySamples(); Light gives 0", "", true, false, "" };
    std::string text;
    for (int engine = 0; engine < 2; ++engine)
    {
        text += engine == 0 ? "Light" : "; Full";
        for (double fs : rates)
        {
            Plugin pl (fs);
            pl.set (ids::engine, (float) engine);
            pl.set (ids::width, 0);
            pl.prepare();
            const auto y = pl.render (impulse (3 * 8192));
            size_t at = 0;
            float best = -1.0f;
            for (size_t i = 0; i < y.l.size(); ++i)
            {
                const float v = std::abs (0.5f * (y.l[i] + y.r[i]));
                if (v > best)
                {
                    best = v;
                    at = i;
                }
            }
            const bool ok = (int) at == pl.latency() && (engine == 1 || at == 0);
            r.pass = r.pass && ok;
            text += " " + std::to_string (at) + (ok ? "" : "(reported " + std::to_string (pl.latency()) + ")");
        }
    }
    r.measured = text + " samples at 44.1/48/96 kHz";
    return r;
}

Result t4SpreadFlat()
{
    Result r { "T4", "Spread: |L|^2 + |R|^2 flat within 0.1 dB from 20 Hz to 20 kHz, both types", "", true, false, "" };
    std::string text;
    for (int type : { 0, 1 })
    {
        std::vector<double> l2, r2;
        double binHz = 0;
        spreadResponse (type, l2, r2, binHz);
        double lo = 1e300, hi = -1e300;
        for (size_t k = 0; k < l2.size(); ++k)
        {
            const double f = (double) k * binHz;
            if (f < 20.0 || f > 20000.0)
                continue;
            const double db = 10.0 * std::log10 (l2[k] + r2[k]);
            lo = std::min (lo, db);
            hi = std::max (hi, db);
        }
        const double ripple = hi - lo;
        r.pass = r.pass && ripple <= 0.1;
        text += std::string (type == 0 ? "Delay " : ", Cascade ") + fmt (ripple, 4) + " dB";
    }
    r.measured = text;
    return r;
}

Result t5SpreadComplementary()
{
    Result r { "T5", "Spread: Pearson correlation of |L|^2 and |R|^2 at 400 log points 50 Hz - 15 kHz <= -0.9, both types", "", true, false, "" };
    std::string text;
    for (int type : { 0, 1 })
    {
        std::vector<double> l2, r2;
        double binHz = 0;
        spreadResponse (type, l2, r2, binHz);
        std::vector<double> a, b;
        for (int i = 0; i < 400; ++i)
        {
            const double f = 50.0 * std::pow (15000.0 / 50.0, i / 399.0);
            const size_t k = (size_t) std::lround (f / binHz);
            a.push_back (l2[k]);
            b.push_back (r2[k]);
        }
        const double c = pearson (a, b);
        r.pass = r.pass && c <= -0.9;
        text += std::string (type == 0 ? "Delay " : ", Cascade ") + fmt (c, 4);
    }
    r.measured = text;
    return r;
}

Result t6Haas()
{
    Result r { "T6", "Preset 2 on noise: L - input <= -100 dB; R - input delayed 720 samples <= -60 dB", "", false, false, "" };
    Plugin pl;
    pl.preset (2);
    pl.prepare();
    const Signal x = noise (samples (3.0, pl.fs));
    const auto y = pl.render (x);
    const size_t from = (size_t) samples (1.0, pl.fs);
    const double el = errorDb (y.l, x, 1.0, 0, x, from, x.size());
    const double er = errorDb (y.r, x, 1.0, 720, x, from, x.size());
    r.pass = el <= -100.0 && er <= -60.0;
    r.measured = "L " + fmtDb (el) + ", R " + fmtDb (er);
    return r;
}

Result t16Guard()
{
    Result r { "T16", "All generators at 100 %, width 200 %, guard On, noise: correlation of every 100 ms window after 500 ms >= -0.1", "", false, true, "" };
    Plugin pl;
    for (const char* id : { ids::spread_amount, ids::delay_amount, ids::mod_amount, ids::velvet_amount, ids::pan_amount })
        pl.set (id, 100);
    pl.set (ids::width, 200);
    pl.set (ids::guard, 1);
    pl.prepare();
    const Signal x = noise (samples (5.0, pl.fs));
    const auto y = pl.render (x);
    const size_t win = (size_t) samples (0.1, pl.fs);
    double worst = 1.0;
    for (size_t from = (size_t) samples (0.5, pl.fs); from + win <= x.size(); from += win)
        worst = std::min (worst, correlation (y.l, y.r, from, from + win));
    r.pass = worst >= -0.1;
    r.measured = "minimum " + fmt (worst, 3);
    return r;
}

Result t27Bypass()
{
    Result r { "T27", "Bypass on, stereo noise: output - input delayed by Lat <= -120 dB, both engines", "", true, false, "" };
    std::string text;
    for (int engine = 0; engine < 2; ++engine)
    {
        Plugin pl (48000.0, 512, 2);
        pl.set (ids::engine, (float) engine);
        pl.set (ids::bypass, 1);
        pl.prepare();
        const Signal xl = noise (samples (2.0, pl.fs), 1), xr = noise (samples (2.0, pl.fs), 2);
        const auto y = pl.render (xl, &xr);
        const int lat = pl.latency();
        const double el = errorDb (y.l, xl, 1.0, lat, xl, (size_t) lat, xl.size());
        const double er = errorDb (y.r, xr, 1.0, lat, xr, (size_t) lat, xr.size());
        const double worst = std::max (el, er);
        r.pass = r.pass && worst <= -120.0;
        text += std::string (engine == 0 ? "Light " : ", Full ") + fmtDb (worst) + " (Lat " + std::to_string (lat) + ")";
    }
    r.measured = text;
    return r;
}
} // namespace sph::measure
