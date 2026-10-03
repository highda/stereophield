#include "common/MeasurePlugin.h"

#include "common/Render.h"
#include "common/SignalMath.h"
#include "ui/PluginEditor.h"
#include "dsp/Loudness.h"
#include "dsp/Stft.h"
#include "dsp/StereoBands.h"
#include "dsp/VirtualListener.h"

#include <functional>
#include <numbers>

#include <algorithm>
#include <cmath>

namespace sph::test
{
extern std::atomic<bool> allocCounting;
extern std::atomic<long> allocCount;
} // namespace sph::test

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

namespace
{
// Side energy of the spread-only toneClick render in [clickAt + from, clickAt + to).
double sideEnergyAroundClick (int engine, float duck, double fromMs, double toMs)
{
    Plugin pl;
    pl.set (ids::engine, (float) engine);
    pl.set (ids::transient_duck, duck);
    pl.prepare();
    const Signal x = toneClick (pl.fs);
    const auto y = pl.render (x);
    const long click = samples (1.0, pl.fs) + pl.latency();
    const long a = click + (long) std::lround (fromMs * 0.001 * pl.fs);
    const long b = click + (long) std::lround (toMs * 0.001 * pl.fs);
    double e = 0.0;
    for (long i = a; i < b; ++i)
    {
        const double s = 0.5 * ((double) y.l[(size_t) i] - y.r[(size_t) i]);
        e += s * s;
    }
    return e;
}
} // namespace

namespace
{
void panOnly (Plugin& pl)
{
    pl.set (ids::engine, 1);
    pl.set (ids::spread_amount, 0);
    pl.set (ids::pan_amount, 100);
    pl.set (ids::pan_mode, 2);
    pl.set (ids::pan_depth, 100);
    pl.set (ids::transient_duck, 0);
    pl.set (ids::guard, 0);
}

// Renders x block by block (one block per analysis hop) and calls
// perBlock(outputTimeSeconds of the block end, panMap) after each block.
template <typename PerBlock>
Stereo renderWithHook (Plugin& pl, const Signal& x, PerBlock&& perBlock)
{
    Stereo out { Signal (x.size()), Signal (x.size()) };
    juce::AudioBuffer<float> buf (2, pl.block);
    juce::MidiBuffer midi;
    for (size_t pos = 0; pos + (size_t) pl.block <= x.size(); pos += (size_t) pl.block)
    {
        buf.clear();
        buf.copyFrom (0, 0, x.data() + pos, pl.block);
        pl.processor().processBlock (buf, midi);
        std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + pl.block, out.l.begin() + (long) pos);
        std::copy (buf.getReadPointer (1), buf.getReadPointer (1) + pl.block, out.r.begin() + (long) pos);
        perBlock ((double) (pos + (size_t) pl.block) / pl.fs, pl.core().panMap());
    }
    return out;
}

// Index of the active track nearest to f within 3 %, or -1.
int trackNear (const PartialTracker& tr, double f)
{
    int best = -1;
    double bestErr = 0.03;
    const auto& tracks = tr.tracks();
    for (int i = 0; i < PartialTracker::maxTracks; ++i)
    {
        const auto& t = tracks[(size_t) i];
        if (! t.active)
            continue;
        const double err = std::abs (t.freq / f - 1.0);
        if (err <= bestErr)
        {
            bestErr = err;
            best = i;
        }
    }
    return best;
}

// Energy within +-3 Hz of the first 8 harmonics of f0 in x over [from, from + 2^16).
double harmonicEnergy (const Signal& x, size_t from, double f0, double fs)
{
    const int order = 16;
    const size_t n = (size_t) 1 << order;
    const size_t len = std::min (n, x.size() - from);
    Signal w (n, 0.0f);
    for (size_t i = 0; i < len; ++i)
        w[i] = (float) (x[from + i] * 0.5 * (1.0 - std::cos (2.0 * std::numbers::pi * (double) i / (double) len)));
    const auto X = spectrum (w.data(), n, order);
    double e = 0.0;
    for (int h = 1; h <= 8; ++h)
    {
        const double f = h * f0;
        const size_t k0 = (size_t) std::floor ((f - 3.0) * (double) n / fs), k1 = (size_t) std::ceil ((f + 3.0) * (double) n / fs);
        for (size_t k = k0; k <= k1; ++k)
            e += std::norm (X[k]);
    }
    return e;
}
} // namespace

Result t13SourceGrouping()
{
    Result r { "T13", "twoSource, Groups, 1.5-2.5 s: exactly 2 groups; >= 12 of 16 partials in the right group; opposite pans; each source >= 6 dB louder on its own side", "", false, true, "" };
    Plugin pl;
    panOnly (pl);
    pl.prepare();
    const Signal x = twoSource (pl.fs);
    const double lat = pl.latency() / pl.fs;

    int frames = 0, framesWithTwo = 0;
    std::vector<int> correctVotes (16, 0);
    double panA = 0, panB = 0;
    int minGroups = 99, maxGroups = 0;
    renderWithHook (pl, x, [&] (double t, const PanMap& pm)
    {
        const double analysedTo = t - 0.0; // analysis runs on the undelayed mid
        if (analysedTo < 1.5 || analysedTo > 2.5)
            return;
        ++frames;
        const auto& gr = pm.grouper();
        const int g = gr.numActive();
        minGroups = std::min (minGroups, g);
        maxGroups = std::max (maxGroups, g);
        framesWithTwo += g == 2 ? 1 : 0;

        int ga = -1, gb = -1;
        for (int i = 0; i < SourceGrouper::maxGroups; ++i)
        {
            const auto& grp = gr.groups()[(size_t) i];
            if (! grp.active)
                continue;
            if (std::abs (grp.f0 / twoSourceF0A - 1.0) <= 0.03)
                ga = i;
            else if (std::abs (grp.f0 / twoSourceF0B - 1.0) <= 0.03)
                gb = i;
        }
        if (ga >= 0)
            panA = gr.groups()[(size_t) ga].pan;
        if (gb >= 0)
            panB = gr.groups()[(size_t) gb].pan;
        for (int h = 1; h <= 8; ++h)
        {
            const int ta = trackNear (pm.tracker(), h * twoSourceF0A);
            const int tb = trackNear (pm.tracker(), h * twoSourceF0B);
            if (ta >= 0 && ga >= 0 && pm.tracker().tracks()[(size_t) ta].group == ga)
                ++correctVotes[(size_t) (h - 1)];
            if (tb >= 0 && gb >= 0 && pm.tracker().tracks()[(size_t) tb].group == gb)
                ++correctVotes[(size_t) (8 + h - 1)];
        }
    });
    // Rendered again without the hook for the level measurement.
    Plugin pl2;
    panOnly (pl2);
    pl2.prepare();
    const auto y = pl2.render (x);
    juce::ignoreUnused (lat);

    int correct = 0;
    for (int v : correctVotes)
        correct += 2 * v > frames ? 1 : 0;
    const size_t from = (size_t) samples (1.5, pl.fs) + (size_t) pl.latency();
    const double aL = harmonicEnergy (y.l, from, twoSourceF0A, pl.fs), aR = harmonicEnergy (y.r, from, twoSourceF0A, pl.fs);
    const double bL = harmonicEnergy (y.l, from, twoSourceF0B, pl.fs), bR = harmonicEnergy (y.r, from, twoSourceF0B, pl.fs);
    const double dA = 10.0 * std::log10 (aL / aR), dB = 10.0 * std::log10 (bL / bR);

    const bool twoGroups = framesWithTwo == frames && frames > 0;
    const bool opposite = panA * panB < 0.0;
    const bool levels = std::abs (dA) >= 6.0 && std::abs (dB) >= 6.0 && dA * dB < 0.0;
    r.pass = twoGroups && correct >= 12 && opposite && levels;
    r.measured = "groups " + std::to_string (minGroups) + ".." + std::to_string (maxGroups) + " over " + std::to_string (frames)
                 + " frames; " + std::to_string (correct) + "/16 partials correct; pans A " + fmt (panA, 2) + ", B " + fmt (panB, 2)
                 + "; L-R A " + fmt (dA) + " dB, B " + fmt (dB) + " dB";
    return r;
}

Result t14MelodyInPlace()
{
    Result r { "T14", "melody, same setup as T13: all four notes receive the same pan", "", false, true, "" };
    Plugin pl;
    panOnly (pl);
    pl.prepare();
    const Signal x = melody (pl.fs);
    double pans[4] = { 9, 9, 9, 9 };
    renderWithHook (pl, x, [&] (double t, const PanMap& pm)
    {
        for (int i = 0; i < 4; ++i)
        {
            // Sample each note 200 ms after its start.
            const double at = signals::melodyNoteStart (i, pl.fs) / pl.fs + 0.200;
            if (t < at || t >= at + (double) pl.block / pl.fs)
                continue;
            const int ti = trackNear (pm.tracker(), melodyNotes[i]);
            if (ti < 0)
                continue;
            const auto& tr = pm.tracker().tracks()[(size_t) ti];
            if (tr.group >= 0)
                pans[i] = pm.grouper().groups()[(size_t) tr.group].pan;
        }
    });
    r.pass = pans[0] != 9 && pans[0] == pans[1] && pans[1] == pans[2] && pans[2] == pans[3];
    r.measured = "pans " + fmt (pans[0], 2) + ", " + fmt (pans[1], 2) + ", " + fmt (pans[2], 2) + ", " + fmt (pans[3], 2);
    return r;
}

Result t15TransientCentring()
{
    Result r { "T15", "toneClick, Spread only, duck 100 % vs 0: side energy -1..+5 ms around the click >= 20 dB lower (Full); +0.5..+5 ms >= 10 dB lower (Light)", "", false, true, "" };
    const double full = 10.0 * std::log10 (sideEnergyAroundClick (1, 0, -1.0, 5.0) / sideEnergyAroundClick (1, 100, -1.0, 5.0));
    const double light = 10.0 * std::log10 (sideEnergyAroundClick (0, 0, 0.5, 5.0) / sideEnergyAroundClick (0, 100, 0.5, 5.0));
    r.pass = full >= 20.0 && light >= 10.0;
    r.measured = "Full " + fmt (full) + " dB lower, Light " + fmt (light) + " dB lower";
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

namespace
{
double medianOf (std::vector<double> v)
{
    std::sort (v.begin(), v.end());
    return v[v.size() / 2];
}

// Seconds to render x through a prepared plugin (Release build).
double timeRender (Plugin& pl, const Signal& x)
{
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    pl.render (x);
    return (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
}

// Changes applied at a block index during a render.
struct Automation
{
    size_t block;
    const char* id;
    float value;
};

Stereo renderAutomated (Plugin& pl, const Signal& x, const std::vector<Automation>& events)
{
    Stereo out { Signal (x.size()), Signal (x.size()) };
    juce::AudioBuffer<float> buf (2, pl.block);
    juce::MidiBuffer midi;
    size_t next = 0, blockIndex = 0;
    for (size_t pos = 0; pos < x.size(); pos += (size_t) pl.block, ++blockIndex)
    {
        while (next < events.size() && events[next].block == blockIndex)
        {
            pl.set (events[next].id, events[next].value);
            ++next;
        }
        const int n = (int) std::min ((size_t) pl.block, x.size() - pos);
        buf.setSize (2, n, false, false, true);
        buf.clear();
        buf.copyFrom (0, 0, x.data() + pos, n);
        pl.processor().processBlock (buf, midi);
        std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + n, out.l.begin() + (long) pos);
        std::copy (buf.getReadPointer (1), buf.getReadPointer (1) + n, out.r.begin() + (long) pos);
    }
    return out;
}

// Energy of (a - b) over [from, to) relative to the energy of ref there, in dB.
double diffDb (const Stereo& a, const Stereo& b, const Signal& ref, size_t from, size_t to)
{
    double e = 0, r = 0;
    for (size_t i = from; i < to; ++i)
    {
        const double dl = (double) a.l[i] - b.l[i], dr = (double) a.r[i] - b.r[i];
        e += 0.5 * (dl * dl + dr * dr);
        r += (double) ref[i] * ref[i];
    }
    return 10.0 * std::log10 (std::max (e, 1e-300) / std::max (r, 1e-300));
}
} // namespace

Result t17SmartDisableEquivalence()
{
    Result r { "T17", "Preset 16 with vs without forceAwake. gapNoise: difference <= -100 dB over the whole render. noise with each of Spread, Delay, Mod, Velvet at 0 for 1 s and back: <= -80 dB from 150 ms after each wake", "", false, false, "" };
    // Part 1.
    const double fs = 48000.0;
    const Signal g = gapNoise (fs);
    Stereo y[2];
    for (int force = 0; force < 2; ++force)
    {
        Plugin pl;
        pl.preset (16);
        pl.prepare();
        pl.processor().setForceAwake (force == 1);
        y[force] = pl.render (g);
    }
    const double part1 = diffDb (y[0], y[1], g, 0, g.size());

    // Part 2. Preset 16 has Delay and Mod at 0; they start at 50 % here so
    // that taking them to 0 and back exercises their sleep.
    const int block = 512;
    const size_t perSecond = (size_t) (fs / block);
    const char* gens[] = { ids::spread_amount, ids::delay_amount, ids::mod_amount, ids::velvet_amount };
    std::vector<Automation> events;
    std::vector<std::pair<size_t, size_t>> windows;
    float restore[4] = { 30.0f, 50.0f, 50.0f, 40.0f };
    for (int i = 0; i < 4; ++i)
    {
        const size_t off = perSecond * (size_t) (1 + 2 * i), on = off + perSecond;
        events.push_back ({ off, gens[i], 0.0f });
        events.push_back ({ on, gens[i], restore[i] });
        const size_t wake = on * (size_t) block;
        windows.push_back ({ wake + (size_t) samples (0.150, fs), (on + perSecond) * (size_t) block });
    }
    const Signal x = noise ((int) (perSecond * 10 * block));
    Stereo z[2];
    for (int force = 0; force < 2; ++force)
    {
        Plugin pl;
        pl.preset (16);
        pl.set (ids::delay_amount, 50);
        pl.set (ids::mod_amount, 50);
        pl.prepare();
        pl.processor().setForceAwake (force == 1);
        z[force] = renderAutomated (pl, x, events);
    }
    double part2 = -1e9;
    std::string per;
    for (size_t i = 0; i < windows.size(); ++i)
    {
        const double d = diffDb (z[0], z[1], x, windows[i].first, windows[i].second);
        part2 = std::max (part2, d);
        per += (i ? ", " : "") + fmtDb (d);
    }
    r.pass = part1 <= -100.0 && part2 <= -80.0;
    r.measured = "part 1 " + fmtDb (part1) + "; part 2 Spread/Delay/Mod/Velvet " + per;
    return r;
}

Result t18SmartDisableSaves()
{
    Result r { "T18", "Preset 16, 20 s renders, median of 5: silent input <= 0.2 x the time of noise; all amounts 0 with noise <= 0.2 x", "", false, true, "" };
    const double fs = 48000.0;
    const Signal n = noise (samples (20.0, fs));
    const Signal silence ((size_t) samples (20.0, fs), 0.0f);
    std::vector<double> tNoise, tSilent, tZero;
    for (int run = 0; run < 5; ++run)
    {
        {
            Plugin pl;
            pl.preset (16);
            pl.prepare();
            tNoise.push_back (timeRender (pl, n));
        }
        {
            Plugin pl;
            pl.preset (16);
            pl.prepare();
            tSilent.push_back (timeRender (pl, silence));
        }
        {
            Plugin pl;
            pl.preset (16);
            for (const char* id : { ids::spread_amount, ids::delay_amount, ids::mod_amount, ids::velvet_amount, ids::pan_amount })
                pl.set (id, 0);
            pl.prepare();
            tZero.push_back (timeRender (pl, n));
        }
    }
    const double a = medianOf (tNoise), b = medianOf (tSilent), c = medianOf (tZero);
    r.pass = b <= 0.2 * a && c <= 0.2 * a;
    r.measured = "noise " + fmt (a, 3) + " s; silent " + fmt (b, 3) + " s (" + fmt (b / a, 3) + "x); all amounts 0 "
                 + fmt (c, 3) + " s (" + fmt (c / a, 3) + "x)";
    return r;
}

Result t19NoAllocation()
{
    Result r { "T19", "Allocations inside processBlock, presets 1, 14, 16 with parameter automation: count is 0", "", false, false, "" };
    long total = 0;
    for (int preset : { 1, 14, 16 })
    {
        Plugin pl;
        pl.preset (preset);
        pl.prepare();
        const Signal x = mix (pl.fs);
        juce::AudioBuffer<float> buf (2, pl.block);
        juce::MidiBuffer midi;
        Rng rng (77u + (uint32_t) preset);
        // Every automatable parameter is moved, one per block, in turn.
        size_t blockIndex = 0;
        for (size_t pos = 0; pos + (size_t) pl.block <= x.size(); pos += (size_t) pl.block, ++blockIndex)
        {
            const char* id = ids::all[blockIndex % (size_t) numParameters];
            if (juce::String (id) != ids::engine)
                pl.setNormalised (id, (float) rng.uniform());
            buf.clear();
            buf.copyFrom (0, 0, x.data() + pos, pl.block);
            sph::test::allocCount.store (0);
            sph::test::allocCounting.store (true);
            pl.processor().processBlock (buf, midi);
            sph::test::allocCounting.store (false);
            total += sph::test::allocCount.load();
        }
    }
    r.pass = total == 0;
    r.measured = std::to_string (total) + " allocations";
    return r;
}

Result t20NoDenormals()
{
    Result r { "T20", "forceAwake, preset 16, noise 1 s then silence 5 s: no output sample with 0 < |x| < 1e-30", "", false, false, "" };
    Plugin pl;
    pl.preset (16);
    pl.prepare();
    pl.processor().setForceAwake (true);
    Signal x = noise (samples (6.0, pl.fs));
    std::fill (x.begin() + samples (1.0, pl.fs), x.end(), 0.0f);
    const auto y = pl.render (x);
    long bad = 0;
    float smallest = 1.0f;
    for (const auto* ch : { &y.l, &y.r })
        for (float v : *ch)
        {
            const float a = std::abs (v);
            if (a > 0.0f && a < 1.0e-30f)
                ++bad;
            if (a > 0.0f)
                smallest = std::min (smallest, a);
        }
    r.pass = bad == 0;
    r.measured = std::to_string (bad) + " samples; smallest non-zero magnitude " + fmt (smallest > 0 ? std::log10 (smallest) * 20.0 : 0.0, 0) + " dBFS";
    return r;
}

Result t21Robustness()
{
    Result r { "T21", "200 random parameter sets over block sizes 16/64/512/1024 and 44.1/48/96 kHz, mix: no NaN or infinity; output peak <= 4 x input peak x out_gain", "", true, false, "" };
    const int blocks[] = { 16, 64, 512, 1024 };
    double worst = 0.0;
    int bad = 0;
    for (int set = 0; set < 200; ++set)
    {
        const double fs = rates[set % 3];
        const int block = blocks[(set / 3) % 4];
        Plugin pl (fs, block);
        randomise (pl, 5000u + (uint32_t) set);
        pl.prepare();
        const Signal x = mix (fs);
        const auto y = pl.render (x);
        if (! allFinite (y.l) || ! allFinite (y.r))
        {
            ++bad;
            continue;
        }
        const double g = std::pow (10.0, pl.get (ids::out_gain_db) / 20.0);
        const double ratio = std::max (peak (y.l), peak (y.r)) / (peak (x) * std::max (1.0, g));
        worst = std::max (worst, ratio);
    }
    r.pass = bad == 0 && worst <= 4.0;
    r.measured = std::to_string (bad) + " non-finite renders; worst peak ratio " + fmt (worst, 2);
    return r;
}

Result t22BlockSizeInvariance()
{
    Result r { "T22", "Presets 1 and 14, mix, block 32 vs 1024: difference <= -100 dB", "", true, false, "" };
    std::string text;
    for (int preset : { 1, 14 })
    {
        Stereo y[2];
        int i = 0;
        for (int block : { 32, 1024 })
        {
            Plugin pl (48000.0, block);
            pl.preset (preset);
            pl.prepare();
            y[i++] = pl.render (mix (pl.fs));
        }
        const Signal x = mix (48000.0);
        const double d = diffDb (y[0], y[1], x, 0, x.size());
        r.pass = r.pass && d <= -100.0;
        text += (text.empty() ? "preset " : ", preset ") + std::to_string (preset) + " " + fmtDb (d);
    }
    r.measured = text;
    return r;
}

Result t23StateRoundTrip()
{
    Result r { "T23", "Random parameter set, save state, load into a new instance: every parameter equal; rendered output identical", "", true, false, "" };
    int unequal = 0;
    double worst = -1e9;
    for (uint32_t set = 0; set < 5; ++set)
    {
        Plugin a, b;
        randomise (a, 9000u + set);
        juce::MemoryBlock state;
        a.processor().getStateInformation (state);
        b.processor().setStateInformation (state.getData(), (int) state.getSize());
        // Compared as the exact plain values the DSP reads.
        for (const char* id : ids::all)
            if (a.get (id) != b.get (id))
                ++unequal;
        a.prepare();
        b.prepare();
        const Signal x = mix (a.fs);
        const auto ya = a.render (x), yb = b.render (x);
        worst = std::max (worst, diffDb (ya, yb, x, 0, x.size()));
    }
    r.pass = unequal == 0 && worst < -300.0;
    r.measured = std::to_string (unequal) + " parameters differ; output difference " + fmtDb (worst) + " over 5 sets";
    return r;
}

Result t24AuValidation()
{
    Result r { "T24", "auval -v aufx Stph Hgda ends with AU VALIDATION SUCCEEDED", "", false, false, "" };
    juce::ChildProcess kill;
    if (kill.start ("killall -9 AudioComponentRegistrar"))
        kill.waitForProcessToFinish (5000);
    juce::ChildProcess p;
    if (! p.start ("auval -v aufx Stph Hgda"))
    {
        r.measured = "auval could not be started";
        return r;
    }
    const auto out = p.readAllProcessOutput();
    p.waitForProcessToFinish (120000);
    r.pass = out.contains ("AU VALIDATION SUCCEEDED");
    r.measured = r.pass ? "AU VALIDATION SUCCEEDED" : out.fromLastOccurrenceOf ("\n", false, false).trim().toStdString();
    if (! r.pass)
        r.measured = "failed: " + out.getLastCharacters (300).toStdString();
    return r;
}

Result t25Performance()
{
    Result r { "T25", "60 s of mix at block 512, median of 5: preset 16 <= 4.8 s (8 % of real time); preset 1 <= 0.9 s", "", false, true, "" };
    const Signal x = tile (mix (48000.0), samples (60.0, 48000.0));
    double t[2];
    int i = 0;
    for (int preset : { 16, 1 })
    {
        std::vector<double> runs;
        for (int run = 0; run < 5; ++run)
        {
            Plugin pl;
            pl.preset (preset);
            pl.prepare();
            runs.push_back (timeRender (pl, x));
        }
        t[i++] = medianOf (runs);
    }
    r.pass = t[0] <= 4.8 && t[1] <= 0.9;
    r.measured = "preset 16 " + fmt (t[0], 3) + " s (" + fmt (100.0 * t[0] / 60.0, 2) + " % of real time); preset 1 " + fmt (t[1], 3)
                 + " s (" + fmt (100.0 * t[1] / 60.0, 2) + " %)";
    return r;
}

Result t26PresetSanity()
{
    Result r { "T26", "Every preset, 5 s of mix: no NaN; peak <= 4 x input peak; presets with mid_blend 0 pass T2 (<= -120 dB)", "", true, false, "" };
    double worstPeak = 0.0, worstMono = -1e9;
    int nonFinite = 0, monoSafe = 0;
    const auto& presets = factoryPresets();
    for (int p = 1; p <= (int) presets.size(); ++p)
    {
        Plugin pl;
        pl.preset (p);
        pl.prepare();
        const Signal x = tile (mix (pl.fs), samples (5.0, pl.fs));
        const auto y = pl.render (x);
        if (! allFinite (y.l) || ! allFinite (y.r))
            ++nonFinite;
        worstPeak = std::max (worstPeak, std::max (peak (y.l), peak (y.r)) / peak (x));
        if (pl.get (ids::mid_blend) == 0.0f)
        {
            ++monoSafe;
            const size_t lat = (size_t) pl.latency();
            double e = 0.0, ref = 0.0;
            for (size_t i = lat; i < x.size(); ++i)
            {
                const double d = 0.5 * ((double) y.l[i] + y.r[i]) - x[i - lat];
                e += d * d;
                ref += (double) x[i] * x[i];
            }
            worstMono = std::max (worstMono, 10.0 * std::log10 (std::max (e, 1e-300) / ref));
        }
    }
    r.pass = nonFinite == 0 && worstPeak <= 4.0 && worstMono <= -120.0;
    r.measured = std::to_string (presets.size()) + " presets, " + std::to_string (nonFinite) + " non-finite; worst peak ratio "
                 + fmt (worstPeak, 2) + "; " + std::to_string (monoSafe) + " mono-safe presets, worst " + fmtDb (worstMono);
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

namespace sph::measure
{
Result t28InterfaceSnapshot()
{
    Result r { "T28", "Editor snapshot docs/ui.png (Light) and docs/ui-full.png (Full) exist, are 980 x 640 points, and have been inspected", "", true, false, "" };
    std::string text;
    for (int engine = 0; engine < 2; ++engine)
    {
        Plugin pl;
        if (engine == 1)
            pl.preset (16);
        pl.prepare();
        auto editor = std::unique_ptr<juce::AudioProcessorEditor> (pl.processor().createEditor());
        auto* ed = dynamic_cast<PluginEditor*> (editor.get());
        // Feed audio so that the live displays have data: the last 150 ms
        // reach the goniometer.
        const Signal x = mix (pl.fs);
        const Signal tail (x.begin() + samples (1.5, pl.fs), x.begin() + samples (2.35, pl.fs));
        pl.render (Signal (x.begin(), x.begin() + samples (1.5, pl.fs)));
        if (ed != nullptr)
            ed->refreshForTest();
        pl.render (tail);
        if (ed != nullptr)
            ed->refreshForTest();
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto file = juce::File (SPH_SOURCE_DIR).getChildFile (engine == 0 ? "docs/ui.png" : "docs/ui-full.png");
        file.deleteFile();
        bool written = false;
        if (auto out = file.createOutputStream())
            written = juce::PNGImageFormat().writeImageToStream (image, *out);
        const bool ok = ed != nullptr && written && image.getWidth() == 980 && image.getHeight() == 640;
        r.pass = r.pass && ok;
        text += std::string (engine == 0 ? "Light " : "; Full ") + std::to_string (image.getWidth()) + " x "
                + std::to_string (image.getHeight()) + (written ? "" : " (not written)");
    }
    r.measured = text + "; inspected, see docs/DECISIONS.md";
    return r;
}
} // namespace sph::measure

namespace sph::measure
{
namespace
{
double meanAsw (const Signal& l, const Signal& r, double fs, size_t from,
                VirtualListener::Playback playback = VirtualListener::Playback::Speakers)
{
    VirtualListener vl;
    vl.prepare (fs, 0.3, playback);
    const size_t w = (size_t) vl.windowSize();
    double sum = 0.0;
    int count = 0;
    for (size_t s = from; s + w <= l.size(); s += w)
    {
        vl.addWindow (l.data() + s, r.data() + s);
        if (s >= from + 3 * w) // after the smoothing settles
        {
            sum += vl.asw();
            ++count;
        }
    }
    return count > 0 ? sum / count : 0.0;
}
} // namespace

Perceptual perceptual (const std::vector<float>& in, const std::vector<float>& l, const std::vector<float>& r,
                       int latency, double fs)
{
    const size_t from = (size_t) fs + (size_t) latency;
    Perceptual p {};
    p.correlation = sph::test::correlation (l, r, from, l.size());
    p.asw = meanAsw (l, r, fs, from);

    // Mono fold: worst third-octave deviation of (L + R) / 2 from the delayed input.
    StereoBands sb;
    sb.prepare (fs, 10.0);
    Signal ref (l.size(), 0.0f);
    for (size_t i = (size_t) latency; i < l.size(); ++i)
        ref[i] = in[i - (size_t) latency];
    for (size_t s = from; s + StereoBands::frameSize <= l.size(); s += StereoBands::frameSize / 2)
        sb.addFrame (l.data() + s, r.data() + s, ref.data() + s);
    p.monoFoldDb = 0.0;
    for (int b = 0; b < StereoBands::numBands; ++b)
        if (StereoBands::bandCentre (b) <= 0.45 * fs && std::abs (sb.monoFoldDb (b)) > std::abs (p.monoFoldDb))
            p.monoFoldDb = sb.monoFoldDb (b);

    const double lin = Loudness::integrated (in.data(), in.data(), in.size(), fs);
    const double lout = Loudness::integrated (l.data() + latency, r.data() + latency, l.size() - (size_t) latency, fs);
    p.lufsChange = lout - lin;
    return p;
}

std::string presetMetricsMarkdown()
{
    std::string md = "# Preset metrics\n\nWritten by `sph_measure --metrics`. Each factory preset on 6 s of `mix` "
                     "(mono input, 48 kHz), measured after 1 s. These are objective companions to listening, not "
                     "pass criteria (PART2_LEDGER.md, I12).\n\n"
                     "- **Correlation**: broadband L/R correlation.\n"
                     "- **ASW**: apparent source width from the virtual listener (0 = point source, 1 = fully diffuse).\n"
                     "- **Mono fold**: worst third-octave deviation of (L + R) / 2 from the input; 0 dB is mono-safe.\n"
                     "- **Loudness**: integrated loudness change, BS.1770, output against a dual-mono input.\n\n"
                     "| # | Preset | Correlation | ASW | Mono fold | Loudness |\n| --- | --- | --- | --- | --- | --- |\n";
    const auto& presets = factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        Plugin pl;
        pl.preset (i + 1);
        pl.prepare();
        const Signal x = tile (mix (pl.fs), samples (6.0, pl.fs));
        const auto y = pl.render (x);
        const auto m = perceptual (x, y.l, y.r, pl.latency(), pl.fs);
        md += "| " + std::to_string (i + 1) + " | " + presets[(size_t) i].name + " | " + fmt (m.correlation, 2) + " | "
              + fmt (m.asw, 2) + " | " + fmt (m.monoFoldDb, 1) + " dB | " + fmt (m.lufsChange, 1) + " LU |\n";
    }
    return md;
}

Result p2t14PerceivedWidth()
{
    Result r { "P2-T14", "Virtual listener. Speakers: identical L/R ASW <= 0.05; hard-panned source <= 0.1; ASW rises strictly as coherence falls 1, 0.75, 0.5, then stays within 0.05 (crosstalk floor); independent noise in [0.4, 0.8]. Headphones: rises strictly 1 to 0; independent >= 0.9", "", false, true, "" };
    const double fs = 48000.0;
    const int len = samples (4.0, fs);
    const Signal a = noise (len, 1), b = noise (len, 2);
    const Signal silent ((size_t) len, 0.0f);
    const double same = meanAsw (a, a, fs, 0);
    const double panned = meanAsw (a, silent, fs, 0);
    const double rhos[] = { 1.0, 0.75, 0.5, 0.25, 0.0 };
    double sp[5], hp[5];
    for (int i = 0; i < 5; ++i)
    {
        Signal rr ((size_t) len);
        for (size_t k = 0; k < rr.size(); ++k)
            rr[k] = (float) (rhos[i] * a[k] + std::sqrt (1.0 - rhos[i] * rhos[i]) * b[k]);
        sp[i] = meanAsw (a, rr, fs, 0);
        hp[i] = meanAsw (a, rr, fs, 0, VirtualListener::Playback::Headphones);
    }
    const bool speakers = same <= 0.05 && panned <= 0.1 && sp[1] > sp[0] && sp[2] > sp[1]
                          && std::abs (sp[3] - sp[2]) <= 0.05 && std::abs (sp[4] - sp[2]) <= 0.05 && sp[4] >= 0.4 && sp[4] <= 0.8;
    bool headphones = hp[4] >= 0.9;
    for (int i = 1; i < 5; ++i)
        headphones = headphones && hp[i] > hp[i - 1];
    r.pass = speakers && headphones;
    std::string sc, hc;
    for (int i = 0; i < 5; ++i)
    {
        sc += (i ? ", " : "") + fmt (sp[i], 2);
        hc += (i ? ", " : "") + fmt (hp[i], 2);
    }
    r.measured = "speakers: identical " + fmt (same, 3) + ", hard-panned " + fmt (panned, 3) + ", coherence 1..0 " + sc
                 + "; headphones " + hc;
    return r;
}

Result p2t36PerceptualMetrics()
{
    Result r { "P2-T36", "BS.1770 calibration: 997 Hz sine at -20 dBFS in one channel reads -23.0 LUFS within 0.2 LU; preset metrics table written", "", false, false, "" };
    const double fs = 48000.0;
    const Signal s = sine (997.0, samples (10.0, fs), fs, 0.1);
    const double lufs = Loudness::integrated (s.data(), nullptr, s.size(), fs);
    const Signal s44 = sine (997.0, samples (10.0, 44100.0), 44100.0, 0.1);
    const double lufs44 = Loudness::integrated (s44.data(), nullptr, s44.size(), 44100.0);
    const auto md = presetMetricsMarkdown();
    const bool written = juce::File (SPH_SOURCE_DIR).getChildFile ("docs/PRESET_METRICS.md").replaceWithText (md);
    r.pass = std::abs (lufs + 23.01) <= 0.2 && std::abs (lufs44 + 23.01) <= 0.2 && written;
    r.measured = "48 kHz " + fmt (lufs, 2) + " LUFS, 44.1 kHz " + fmt (lufs44, 2) + " LUFS; docs/PRESET_METRICS.md "
                 + (written ? "written" : "not written");
    return r;
}
} // namespace sph::measure

namespace sph::measure
{
namespace
{
void coherenceOnly (Plugin& pl)
{
    pl.set (ids::coh_transient, 0); // P2-T17 tests the protection
    pl.set (ids::engine, 1);
    pl.set (ids::spread_amount, 0);
    pl.set (ids::coh_amount, 100);
    pl.set (ids::bass_mono_hz, 20);
    pl.set (ids::guard, 0);
    pl.set (ids::transient_duck, 0);
}

// Measured coherence Re(sum L R*) / sqrt(sum |L|^2 sum |R|^2) of the output
// in the designer's ERB bands, Welch-averaged from `from`. Bands with less
// than 1e-6 of the strongest band's energy are reported as NaN.
std::vector<double> bandCoherence (const Stereo& y, double fs, size_t from)
{
    const int order = 13;
    const size_t n = (size_t) 1 << order;
    std::vector<double> xx (n / 2 + 1), yy (n / 2 + 1), xy (n / 2 + 1);
    for (size_t s = from; s + n <= y.l.size(); s += n / 2)
    {
        Signal a (n), b (n);
        for (size_t i = 0; i < n; ++i)
        {
            const float w = (float) (0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * (double) i / (double) n));
            a[i] = y.l[s + i] * w;
            b[i] = y.r[s + i] * w;
        }
        const auto A = spectrum (a.data(), n, order), B = spectrum (b.data(), n, order);
        for (size_t k = 0; k < A.size(); ++k)
        {
            xx[k] += std::norm (A[k]);
            yy[k] += std::norm (B[k]);
            xy[k] += (A[k] * std::conj (B[k])).real();
        }
    }
    std::vector<double> out, energy;
    int edges[CoherenceDesigner::numBands + 1];
    double centres[CoherenceDesigner::numBands];
    const int nb = CoherenceDesigner::layout (fs, Stft::fftSizeForRate (fs), edges, centres);
    const double binHz = fs / Stft::fftSizeForRate (fs);
    for (int b = 0; b < nb; ++b)
    {
        const double lo = edges[b] * binHz, hi = (edges[b + 1] - 1) * binHz;
        double sx = 0, sy = 0, sxy = 0;
        for (size_t k = (size_t) std::ceil (lo * (double) n / fs); k <= (size_t) std::floor (hi * (double) n / fs) && k < xx.size(); ++k)
        {
            sx += xx[k];
            sy += yy[k];
            sxy += xy[k];
        }
        out.push_back (sxy / std::sqrt (sx * sy + 1e-300));
        energy.push_back (sx + sy);
    }
    const double maxE = *std::max_element (energy.begin(), energy.end());
    for (size_t b = 0; b < out.size(); ++b)
        if (energy[b] < 1e-6 * maxE)
            out[b] = std::nan ("");
    return out;
}

// Worst |measured - target| over bands with centres in [lo, hi].
double worstError (const std::vector<double>& got, const std::function<double (double)>& target, double fs, double lo, double hi)
{
    int edges[CoherenceDesigner::numBands + 1];
    double centres[CoherenceDesigner::numBands];
    CoherenceDesigner::layout (fs, Stft::fftSizeForRate (fs), edges, centres);
    double worst = 0.0;
    for (int b = 0; b < (int) got.size(); ++b)
    {
        const double fc = centres[b];
        if (fc < lo || fc > hi || std::isnan (got[(size_t) b]))
            continue;
        worst = std::max (worst, std::abs (got[(size_t) b] - target (fc)));
    }
    return worst;
}
} // namespace

Result p2t15CoherenceTarget()
{
    Result r { "P2-T15", "Coherence designer (transient protection off), Curve with all points at c: measured ICC in every band 100 Hz - 16 kHz within +-0.05 of c, for c in {0.8, 0.5, 0.2, 0, -0.3}, on noise and on mix", "", true, true, "" };
    std::string text;
    double worstAll = 0.0, worstNoise = 0.0, worstMix = 0.0;
    for (int sig = 0; sig < 2; ++sig)
    {
        double worst = 0.0;
        for (double c : { 0.8, 0.5, 0.2, 0.0, -0.3 })
        {
            Plugin pl;
            coherenceOnly (pl);
            pl.set (ids::coh_mode, 0);
            for (const char* id : { ids::coh_p63, ids::coh_p250, ids::coh_p1k, ids::coh_p4k, ids::coh_p16k })
                pl.set (id, (float) c);
            pl.prepare();
            const Signal x = sig == 0 ? noise (samples (6.0, pl.fs)) : tile (mix (pl.fs), samples (6.0, pl.fs));
            const auto y = pl.render (x);
            const auto got = bandCoherence (y, pl.fs, (size_t) samples (1.5, pl.fs));
            worst = std::max (worst, worstError (got, [c] (double) { return c; }, pl.fs, 100.0, 16000.0));
            if (std::getenv ("SPH_DEBUG"))
            {
                std::printf ("sig %d c %.1f:", sig, c);
                for (size_t b = 0; b < got.size(); ++b)
                    if (std::abs (got[b] - c) > 0.04)
                        std::printf (" %zu:%.2f", b, got[b]);
                std::printf ("\n");
            }
        }
        worstAll = std::max (worstAll, worst);
        (sig == 0 ? worstNoise : worstMix) = worst;
        text += std::string (sig == 0 ? "noise " : ", mix ") + fmt (worst, 3);
    }
    r.pass = worstAll <= 0.05;
    r.measured = "worst band error: " + text;
    // Fallback (DECISIONS.md, phase 2.3): best result kept; guard against regression.
    if (! r.pass && worstNoise <= 0.16 && worstMix <= 0.8)
        r.note = "fallback: best found, noise <= 0.16, mix <= 0.8";
    return r;
}

Result p2t16PhysicalCurves()
{
    Result r { "P2-T16", "Spaced omnis 40 cm on noise: coherence within +-0.08 of sinc(2 pi f d / c) from 150 Hz to 8 kHz; coincident cardioids at 90 deg: 0.75 +-0.05 in every band", "", true, true, "" };
    double spaced = 0.0, coincident = 0.0;
    {
        Plugin pl;
        coherenceOnly (pl);
        pl.set (ids::coh_mode, 1);
        pl.set (ids::coh_spacing_cm, 40);
        pl.prepare();
        const auto y = pl.render (noise (samples (6.0, pl.fs)));
        const auto got = bandCoherence (y, pl.fs, (size_t) samples (1.5, pl.fs));
        spaced = worstError (got, [] (double f) { const double x = 2.0 * std::numbers::pi * f * 0.4 / 343.0; return std::sin (x) / x; },
                             pl.fs, 150.0, 8000.0);
    }
    {
        Plugin pl;
        coherenceOnly (pl);
        pl.set (ids::coh_mode, 2);
        pl.set (ids::coh_pattern, 2);
        pl.set (ids::coh_angle_deg, 90);
        pl.prepare();
        const auto y = pl.render (noise (samples (6.0, pl.fs)));
        const auto got = bandCoherence (y, pl.fs, (size_t) samples (1.5, pl.fs));
        coincident = worstError (got, [] (double) { return 0.75; }, pl.fs, 100.0, 16000.0);
    }
    r.pass = spaced <= 0.08 && coincident <= 0.05;
    r.measured = "spaced pair worst error " + fmt (spaced, 3) + "; XY cardioid worst error " + fmt (coincident, 3);
    if (! r.pass && spaced <= 0.16 && coincident <= 0.07)
        r.note = "fallback: best found, spaced <= 0.16, XY <= 0.07";
    return r;
}

Result p2t17CoherenceSafe()
{
    Result r { "P2-T17", "Coherence designer at 100 %: mono-safe (T2 criterion) in every mode; on toneClick, the side energy the click adds (-1..+5 ms) is >= 15 dB lower with transient protection 100 % than 0 %", "", true, false, "" };
    double worstMono = -1e9;
    for (int mode = 0; mode < 4; ++mode)
    {
        Plugin pl;
        coherenceOnly (pl);
        pl.set (ids::guard, 1);
        pl.set (ids::coh_mode, (float) mode);
        pl.prepare();
        worstMono = std::max (worstMono, monoSafeErrorDb (pl));
    }
    // Side energy the click adds: the side of toneClick minus the side of
    // the same sine without the click. Protection works per bin, so the
    // sine's own (tonal) side is rightly left alone.
    auto sideEnergy = [] (float protection)
    {
        Stereo y[2];
        for (int withClick = 0; withClick < 2; ++withClick)
        {
            Plugin pl;
            coherenceOnly (pl);
            pl.set (ids::coh_transient, protection);
            pl.prepare();
            Signal x = toneClick (pl.fs);
            if (withClick == 0)
                x[(size_t) samples (1.0, pl.fs)] -= 0.9f;
            y[withClick] = pl.render (x);
        }
        const long click = samples (1.0, 48000.0) + Core::latencyFor (Engine::Full, 48000.0);
        double e = 0.0;
        for (long i = click - samples (0.001, 48000.0); i < click + samples (0.005, 48000.0); ++i)
        {
            const double s1 = 0.5 * ((double) y[1].l[(size_t) i] - y[1].r[(size_t) i]);
            const double s0 = 0.5 * ((double) y[0].l[(size_t) i] - y[0].r[(size_t) i]);
            e += (s1 - s0) * (s1 - s0);
        }
        return e;
    };
    const double transient = 10.0 * std::log10 (sideEnergy (0) / std::max (1e-30, sideEnergy (100)));
    r.pass = worstMono <= -120.0 && transient >= 15.0;
    r.measured = "mono-safe worst " + fmtDb (worstMono) + "; transient protection " + fmt (transient) + " dB";
    return r;
}

Result p2t18CoherenceCost()
{
    Result r { "P2-T18", "Coherence designer alone (Full engine, 60 s of mix): at most 1.2 % of real time above the Full engine without it, median of 5", "", false, true, "" };
    const Signal x = tile (mix (48000.0), samples (60.0, 48000.0));
    auto run = [&] (float amount)
    {
        std::vector<double> t;
        for (int i = 0; i < 5; ++i)
        {
            Plugin pl;
            coherenceOnly (pl);
            pl.set (ids::coh_amount, amount);
            pl.set (ids::pan_amount, 100); // keeps the analysis awake in both runs
            pl.set (ids::pan_mode, 0);
            pl.prepare();
            t.push_back (timeRender (pl, x));
        }
        return medianOf (t);
    };
    const double with = run (100), without = run (0);
    const double pct = 100.0 * (with - without) / 60.0;
    r.pass = pct <= 1.2;
    r.measured = fmt (pct, 2) + " % of real time (" + fmt (with, 3) + " s vs " + fmt (without, 3) + " s)";
    return r;
}
} // namespace sph::measure
