#pragma once

// Measurements of Easy mode renders (PART3_LEDGER.md sections 3 and 4.2),
// shared by the P3 tests and sph_easy_opt.

#include "common/Corpus.h"
#include "common/MeasurePlugin.h"
#include "common/Render.h"
#include "dsp/Biquad.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace sph::easytest
{
using signals::Signal;

struct Setting
{
    easy::Macros macros;
    const easy::Table* table = nullptr;     // null: the shipped table
    const easy::Weights* weights = nullptr; // null: the classifier
};

struct Render
{
    test::Stereo y;
    int latency = 0;
};

inline void applyMacros (test::Plugin& pl, const easy::Macros& m)
{
    pl.set (ids::ui_mode, 0);
    pl.set (ids::easy_width, 100.0f * m.width);
    pl.set (ids::easy_character, 100.0f * m.character);
    pl.set (ids::easy_space, 100.0f * m.space);
    pl.set (ids::easy_focus, 100.0f * m.focus);
    pl.set (ids::easy_adapt, m.adapt ? 1.0f : 0.0f);
    pl.set (ids::easy_low_latency, m.lowLatency ? 1.0f : 0.0f);
}

inline Render render (const Signal& x, const Setting& s, double fs = 48000.0)
{
    test::Plugin pl (fs);
    applyMacros (pl, s.macros);
    pl.processor().setEasyOverrides (s.table, s.weights);
    pl.prepare();
    Render r;
    r.y = pl.render (x);
    r.latency = pl.latency();
    return r;
}

struct Metrics
{
    double asw = 0.0;           // virtual listener, loudspeakers
    double minCorrelation = 1.0; // over 100 ms windows of programme after 0.5 s
    double lufsChange = 0.0;     // against the dry input
    double bassSideDb = -200.0;  // side below 80 Hz against the mid below 80 Hz (-200: no bass)
    double transientDb = 0.0;    // protection: side/mid of the body over side/mid of the hit
    double monoErrorDb = -300.0; // (L+R)/2 against the delayed input
};

// Hits of the corpus items with known onsets, in samples (before latency).
inline std::vector<size_t> hits (const std::string& name, size_t length, double fs)
{
    std::vector<size_t> h;
    if (name == "tone and click")
        for (size_t at = (size_t) std::lround (1.0 * fs); at < length; at += (size_t) std::lround (2.0 * fs))
            h.push_back (at);
    if (name == "drum loop")
        for (size_t at = 0; at < length; at += (size_t) std::lround (0.3 * fs))
            h.push_back (at);
    return h;
}

inline Metrics measure (const Signal& x, const Render& r, double fs, const std::vector<size_t>& hitList = {})
{
    Metrics m;
    const auto& l = r.y.l;
    const auto& rr = r.y.r;
    const size_t lat = (size_t) r.latency;
    const auto p = measure::perceptual (x, l, rr, r.latency, fs);
    m.asw = p.asw;
    m.lufsChange = p.lufsChange;

    // Windows of programme: the input within 20 dB of its mean power (just
    // after an abrupt stop only the generators' tails are left, pure side
    // with correlation near -1, which is not programme).
    const size_t w = (size_t) std::lround (0.1 * fs);
    double meanPower = 0.0;
    for (float v : x)
        meanPower += (double) v * v;
    meanPower /= (double) std::max<size_t> (1, x.size());
    for (size_t s = lat + (size_t) std::lround (0.5 * fs); s + w <= l.size(); s += w)
    {
        double lr = 0, ll = 0, r2 = 0, in = 0;
        for (size_t i = s; i < s + w; ++i)
        {
            lr += (double) l[i] * rr[i];
            ll += (double) l[i] * l[i];
            r2 += (double) rr[i] * rr[i];
            in += (double) x[i - lat] * x[i - lat];
        }
        if (in > 1e-2 * meanPower * (double) w && ll > 0.0 && r2 > 0.0)
            m.minCorrelation = std::min (m.minCorrelation, lr / std::sqrt (ll * r2));
    }

    // Bass: fourth-order low-pass at 80 Hz on mid and side.
    Biquad lm[2], ls[2];
    for (auto* b : { &lm[0], &lm[1], &ls[0], &ls[1] })
        b->setCoefficients (Biquad::lowPass (fs, 80.0, 0.7071));
    double bm = 0, bs = 0, am = 0, e = 0, ref = 0;
    const size_t from = lat + (size_t) fs;
    for (size_t i = lat; i < l.size(); ++i)
    {
        const double mid = 0.5 * ((double) l[i] + rr[i]), side = 0.5 * ((double) l[i] - rr[i]);
        const double fm = lm[1].process (lm[0].process (mid)), fsd = ls[1].process (ls[0].process (side));
        if (i >= from)
        {
            bm += fm * fm;
            bs += fsd * fsd;
            am += mid * mid;
        }
        const double d = mid - x[i - lat];
        e += d * d;
        ref += (double) x[i - lat] * x[i - lat];
    }
    // Only where the material has bass (within 30 dB of the whole mid).
    m.bassSideDb = bm > 1e-3 * am ? 10.0 * std::log10 (std::max (bs, 1e-300) / bm) : -200.0;
    m.monoErrorDb = 10.0 * std::log10 (std::max (e, 1e-300) / std::max (ref, 1e-300));

    if (! hitList.empty())
    {
        // Side against mid in the 5 ms after each hit, and in the body of the
        // sound from 50 ms after the hit until 10 ms before the next one.
        std::vector<char> region (l.size(), 0);
        const size_t hw = (size_t) std::lround (0.005 * fs), b0 = (size_t) std::lround (0.05 * fs),
                     guard = (size_t) std::lround (0.01 * fs);
        for (size_t k = 0; k < hitList.size(); ++k)
        {
            const size_t h = hitList[k] + lat;
            const size_t next = k + 1 < hitList.size() ? hitList[k + 1] + lat : l.size() + guard;
            for (size_t i = h; i < std::min (l.size(), h + hw); ++i)
                region[i] = 1;
            for (size_t i = h + b0; i + guard < next && i < l.size(); ++i)
                region[i] = 2;
        }
        double hm = 0, hs = 0, sm = 0, ss = 0;
        for (size_t i = from; i < l.size(); ++i)
        {
            if (region[i] == 0)
                continue;
            const double mid = 0.5 * ((double) l[i] + rr[i]), side = 0.5 * ((double) l[i] - rr[i]);
            (region[i] == 1 ? hm : sm) += mid * mid;
            (region[i] == 1 ? hs : ss) += side * side;
        }
        // Without a side there is nothing to protect.
        m.transientDb = hs + ss < 1e-9 * (hm + sm) ? 100.0
                                                   : 10.0 * std::log10 (((ss + 1e-30) / (sm + 1e-30)) / ((hs + 1e-30) / (hm + 1e-30)));
    }
    return m;
}
} // namespace sph::easytest
