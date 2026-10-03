// Part 3 measurements: Easy mode (PART3_LEDGER.md section 8).

#include "common/EasyMetrics.h"
#include "common/Fixtures.h"
#include "common/MeasurePlugin.h"
#include "common/SignalMath.h"
#include "ui/PluginEditor.h"

#include <atomic>
#include <functional>
#include <thread>

namespace sph::test
{
extern std::atomic<bool> allocCounting;
extern std::atomic<long> allocCount;
} // namespace sph::test

namespace sph::measure
{
using namespace sph::test;
using namespace sph::signals;
using easytest::Setting;

namespace
{
constexpr double fs = 48000.0;

// Runs f(0) .. f(n - 1) on every core.
void parallelFor (size_t n, const std::function<void (size_t)>& f)
{
    std::atomic<size_t> next { 0 };
    std::vector<std::thread> pool;
    const unsigned count = std::max (1u, std::min (std::thread::hardware_concurrency(), 8u));
    for (unsigned t = 0; t < count; ++t)
        pool.emplace_back ([&]
        {
            for (size_t i; (i = next.fetch_add (1)) < n;)
                f (i);
        });
    for (auto& t : pool)
        t.join();
}

const std::vector<corpus::Item>& items()
{
    static const auto all = corpus::items (fs, 4.0);
    return all;
}

easy::Macros defaults()
{
    return {};
}

struct Corner
{
    easy::Macros m;
    size_t item;
    easytest::Metrics metrics;
};

// Width 100 % at the corners of Character, Space and Focus, both latency
// modes, every item (P3-T3, P3-T4); computed once.
const std::vector<Corner>& corners()
{
    static const std::vector<Corner> all = []
    {
        std::vector<Corner> c;
        for (int low = 0; low < 2; ++low)
            for (int k = 0; k < 8; ++k)
                for (size_t i = 0; i < items().size(); ++i)
                {
                    easy::Macros m;
                    m.width = 1.0f;
                    m.character = (float) (k & 1);
                    m.space = (float) ((k >> 1) & 1);
                    m.focus = (float) ((k >> 2) & 1);
                    m.lowLatency = low == 1;
                    c.push_back ({ m, i, {} });
                }
        parallelFor (c.size(), [&] (size_t j)
        {
            const auto& item = items()[c[j].item];
            Setting s;
            s.macros = c[j].m;
            const auto r = easytest::render (item.x, s, fs);
            c[j].metrics = easytest::measure (item.x, r, fs);
        });
        return c;
    }();
    return all;
}

std::string describe (const Corner& c)
{
    return items()[c.item].name + " C" + fmt (100 * c.m.character, 0) + " S" + fmt (100 * c.m.space, 0) + " F"
           + fmt (100 * c.m.focus, 0) + (c.m.lowLatency ? " low latency" : "");
}
} // namespace

Result p3t1EasyMonoSafe()
{
    Result r { "P3-T1", "Easy mode is mono-safe: (L+R)/2 against the delayed input <= -120 dB for 50 random macro settings (both latency modes, Adapt on and off) on every corpus item", "", false, false, "" };
    struct Job
    {
        easy::Macros m;
        size_t item;
        double err = 0;
    };
    std::vector<Job> jobs;
    Rng rng (3003);
    for (int s = 0; s < 50; ++s)
    {
        easy::Macros m;
        m.width = (float) rng.uniform();
        m.character = (float) rng.uniform();
        m.space = (float) rng.uniform();
        m.focus = (float) rng.uniform();
        m.adapt = rng.uniform() < 0.5;
        m.lowLatency = s % 2 == 1;
        for (size_t i = 0; i < items().size(); ++i)
            jobs.push_back ({ m, i });
    }
    parallelFor (jobs.size(), [&] (size_t j)
    {
        const auto& x = items()[jobs[j].item].x;
        const Signal shortX (x.begin(), x.begin() + (long) samples (2.0, fs));
        Setting s;
        s.macros = jobs[j].m;
        const auto rr = easytest::render (shortX, s, fs);
        jobs[j].err = easytest::measure (shortX, rr, fs).monoErrorDb;
    });
    double worst = -1e9;
    for (const auto& j : jobs)
        worst = std::max (worst, j.err);
    r.pass = worst <= -120.0;
    r.measured = "worst " + fmtDb (worst) + " over " + std::to_string (jobs.size()) + " renders";
    return r;
}

Result p3t2LinearWidth()
{
    Result r { "P3-T2", "Width is perceptually linear: on every item (except tone and click, a pure sine without measurable width), Adapt on, other macros at default: ASW rises strictly over Width 0, 10 .. 100 %, R^2 of ASW against Width >= 0.95, ASW at 100 % >= 0.35", "", true, true, "" };
    std::vector<std::array<double, 11>> asw (items().size());
    parallelFor (items().size() * 11, [&] (size_t j)
    {
        const size_t i = j / 11;
        const int w = (int) (j % 11);
        if (items()[i].name == "tone and click")
            return;
        Setting s;
        s.macros = defaults();
        s.macros.width = 0.1f * (float) w;
        const auto rr = easytest::render (items()[i].x, s, fs);
        asw[i][(size_t) w] = easytest::measure (items()[i].x, rr, fs).asw;
    });
    std::string text;
    double worstR2 = 1.0, worstTop = 1.0;
    int otherFailures = 0; // among the items without drums
    for (size_t i = 0; i < items().size(); ++i)
    {
        if (items()[i].name == "tone and click")
            continue;
        bool rising = true;
        double sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0, fall = 0;
        for (int w = 0; w <= 10; ++w)
        {
            const double x = 0.1 * w, y = asw[i][(size_t) w];
            if (w > 0 && y <= asw[i][(size_t) w - 1])
            {
                rising = false;
                fall = std::max (fall, asw[i][(size_t) w - 1] - y);
            }
            sx += x;
            sy += y;
            sxx += x * x;
            sxy += x * y;
            syy += y * y;
        }
        const double n = 11.0;
        const double cov = sxy - sx * sy / n, vx = sxx - sx * sx / n, vy = syy - sy * sy / n;
        const double r2 = vy > 0 ? cov * cov / (vx * vy) : 0.0;
        worstR2 = std::min (worstR2, r2);
        worstTop = std::min (worstTop, asw[i][10]);
        const bool ok = rising && r2 >= 0.95 && asw[i][10] >= 0.35;
        const bool nearlyOk = r2 >= 0.95 && asw[i][10] >= 0.35 && fall <= 0.01;
        otherFailures += (items()[i].name == "drum loop" || items()[i].name == "arrangement") ? 0 : (nearlyOk ? 0 : 1);
        r.pass = r.pass && ok;
        text += (text.empty() ? "" : "; ") + items()[i].name + ": R^2 " + fmt (r2, 3) + ", ASW(100 %) " + fmt (asw[i][10], 2)
                + (rising ? "" : ", falls by " + fmt (fall, 3)) + (ok ? "" : " (fails)");
    }
    r.measured = "worst R^2 " + fmt (worstR2, 3) + ", lowest ASW(100 %) " + fmt (worstTop, 2) + ". " + text;
    if (! r.pass && otherFailures == 0)
        r.note = "Fallback (docs/DECISIONS.md, Part 3): every item without drums has R^2 >= 0.95 and ASW(100 %) >= 0.35 and never "
                 "narrows by more than 0.01 (the metric's resolution where width saturates); the drum loop and the arrangement, kept "
                 "centred on their hits, respond to side gain too differently from the rest to be linearised by one open-loop mapping";
    return r;
}

Result p3t3Correlation()
{
    Result r { "P3-T3", "Correlation >= 0 in every 100 ms window of programme after 0.5 s: every item, Width 100 %, the 8 corners of Character, Space and Focus, both latency modes", "", false, true, "" };
    double worst = 1.0;
    std::string where;
    for (const auto& c : corners())
        if (c.metrics.minCorrelation < worst)
        {
            worst = c.metrics.minCorrelation;
            where = describe (c);
        }
    r.pass = worst >= 0.0;
    r.measured = "lowest " + fmt (worst, 2) + " (" + where + ") over " + std::to_string (corners().size()) + " renders";
    if (! r.pass && worst >= -0.1)
        r.note = "Fallback (docs/DECISIONS.md, Part 3): single 100 ms windows on the drum loop dip below 0, where the side's tails "
                 "outlast a hit faster than the guard's 200 ms power average follows; within the guard bound of T16 (>= -0.1)";
    return r;
}

Result p3t4Loudness()
{
    Result r { "P3-T4", "Loudness change against the dry input <= +1.5 LU (BS.1770) at every setting of P3-T3", "", false, true, "" };
    double worst = -1e9;
    std::string where;
    for (const auto& c : corners())
        if (c.metrics.lufsChange > worst)
        {
            worst = c.metrics.lufsChange;
            where = describe (c);
        }
    r.pass = worst <= 1.5;
    r.measured = "highest " + fmt (worst, 2) + " LU (" + where + ")";
    return r;
}

Result p3t5BassTransients()
{
    Result r { "P3-T5", "Side below 80 Hz >= 12 dB under the mid (items with bass, Width 100 %, defaults and the corners of P3-T3); tone and click and drum loop at Focus 50 %, other macros default, Width 100 %: side/mid within 5 ms of each hit >= 6 dB below the body of the sound", "", false, true, "" };
    double worstBass = -1e9;
    std::string bassWhere;
    for (const auto& c : corners())
        if (c.metrics.bassSideDb > worstBass)
        {
            worstBass = c.metrics.bassSideDb;
            bassWhere = describe (c);
        }
    std::string text;
    double worstTrans = 1e9;
    for (const char* name : { "tone and click", "drum loop" })
        for (const auto& item : items())
            if (item.name == name)
            {
                Setting s;
                s.macros = defaults();
                s.macros.width = 1.0f;
                s.macros.focus = 0.5f;
                const auto rr = easytest::render (item.x, s, fs);
                const auto m = easytest::measure (item.x, rr, fs, easytest::hits (item.name, item.x.size(), fs));
                worstTrans = std::min (worstTrans, m.transientDb);
                text += ", " + item.name + " " + fmt (m.transientDb) + " dB";
            }
    r.pass = worstBass <= -12.0 && worstTrans >= 6.0;
    r.measured = "bass side at most " + fmtDb (worstBass) + " (" + bassWhere + "); transients" + text;
    return r;
}

Result p3t6ExpandInaudible()
{
    Result r { "P3-T6", "Expand: Easy for 2 s, expand, continue: bit-identical to staying in Easy (Adapt off, both latency modes); with Adapt on, the written parameters equal the mapped values exactly", "", true, false, "" };
    const Signal x = tile (mix (fs), samples (4.0, fs));
    std::string text;
    for (int low = 0; low < 2; ++low)
    {
        Stereo out[2];
        for (int expand = 0; expand < 2; ++expand)
        {
            Plugin pl (fs);
            easy::Macros m;
            m.width = 0.8f;
            m.character = 0.6f;
            m.adapt = false;
            m.lowLatency = low == 1;
            easytest::applyMacros (pl, m);
            pl.prepare();
            out[expand] = { Signal (x.size()), Signal (x.size()) };
            juce::AudioBuffer<float> buf (2, pl.block);
            juce::MidiBuffer midi;
            for (size_t pos = 0, b = 0; pos + (size_t) pl.block <= x.size(); pos += (size_t) pl.block, ++b)
            {
                if (expand == 1 && pos == (size_t) samples (2.0, fs) / (size_t) pl.block * (size_t) pl.block)
                {
                    pl.processor().expandToComplete();
                    pl.processor().flushLatencyUpdate();
                }
                buf.clear();
                buf.copyFrom (0, 0, x.data() + pos, pl.block);
                pl.processor().processBlock (buf, midi);
                std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + pl.block, out[expand].l.begin() + (long) pos);
                std::copy (buf.getReadPointer (1), buf.getReadPointer (1) + pl.block, out[expand].r.begin() + (long) pos);
            }
            if (expand == 1)
                r.pass = r.pass && ! pl.processor().isEasyMode();
        }
        const bool same = out[0].l == out[1].l && out[0].r == out[1].r;
        r.pass = r.pass && same;
        text += std::string (low ? "; low latency " : "Full ") + (same ? "bit-identical" : "differs");
    }
    {
        Plugin pl (fs);
        easytest::applyMacros (pl, easy::Macros {});
        pl.prepare();
        pl.render (tile (drumLoop (fs), samples (3.0, fs)));
        const auto mapped = pl.processor().effectiveValues();
        pl.processor().expandToComplete();
        const auto written = pl.processor().snapshot();
        int differ = 0;
        for (int i = 0; i < numCoreParameters; ++i)
            differ += mapped[(size_t) i] != written[(size_t) i] ? 1 : 0;
        r.pass = r.pass && differ == 0;
        text += "; Adapt on: " + std::to_string (differ) + " parameters differ from the mapped values";
    }
    r.measured = text;
    return r;
}

Result p3t7CollapseConfirmation()
{
    Result r { "P3-T7", "Collapse without confirmation leaves Complete mode and every parameter unchanged; with confirmation switches to Easy as one undo point", "", true, false, "" };
    Plugin pl (fs);
    pl.prepare();
    pl.set (ids::spread_amount, 33.0f);
    pl.processor().commitUndoPoint();
    const auto before = pl.processor().snapshot();
    const bool refused = ! pl.processor().collapseToEasy (false);
    const bool unchanged = pl.processor().snapshot() == before && ! pl.processor().isEasyMode();
    const bool done = pl.processor().collapseToEasy (true) && pl.processor().isEasyMode();
    const bool undone = pl.processor().undo() && pl.processor().snapshot() == before;
    r.pass = refused && unchanged && done && undone;
    r.measured = std::string ("without confirmation: ") + (refused && unchanged ? "unchanged" : "changed")
                 + "; with: " + (done ? "Easy" : "not Easy") + "; one undo returns: " + (undone ? "yes" : "no");
    return r;
}

Result p3t8MacroAutomation()
{
    Result r { "P3-T8", "Automating the four macros changes no internal parameter, allocates nothing in processBlock, and the output follows them (side energy at Width 90 % above Width 10 %)", "", true, false, "" };
    Plugin pl (fs);
    easytest::applyMacros (pl, easy::Macros {});
    pl.prepare();
    const auto before = pl.processor().snapshot();
    const Signal x = tile (mix (fs), samples (6.0, fs));
    juce::AudioBuffer<float> buf (2, pl.block);
    juce::MidiBuffer midi;
    Rng rng (88);
    long allocations = 0;
    double side[2] {}, mid[2] {};
    const size_t half = x.size() / 2;
    for (size_t pos = 0, b = 0; pos + (size_t) pl.block <= x.size(); pos += (size_t) pl.block, ++b)
    {
        const bool second = pos >= half;
        pl.set (ids::easy_width, second ? 90.0f : 10.0f);
        pl.setNormalised (ids::easy_character, (float) rng.uniform());
        pl.setNormalised (ids::easy_space, (float) rng.uniform());
        pl.setNormalised (ids::easy_focus, (float) rng.uniform());
        buf.clear();
        buf.copyFrom (0, 0, x.data() + pos, pl.block);
        allocCount.store (0);
        allocCounting.store (true);
        pl.processor().processBlock (buf, midi);
        allocCounting.store (false);
        allocations += allocCount.load();
        if (pos % half > (size_t) samples (1.0, fs))
            for (int i = 0; i < pl.block; ++i)
            {
                const double l = buf.getSample (0, i), rr = buf.getSample (1, i);
                side[second] += 0.25 * (l - rr) * (l - rr);
                mid[second] += 0.25 * (l + rr) * (l + rr);
            }
    }
    const auto after = pl.processor().snapshot();
    int moved = 0;
    for (int i = 0; i < numCoreParameters; ++i)
        moved += before[(size_t) i] != after[(size_t) i] ? 1 : 0;
    const double lo = 10.0 * std::log10 (side[0] / mid[0]), hi = 10.0 * std::log10 (side[1] / mid[1]);
    r.pass = moved == 0 && allocations == 0 && hi > lo + 6.0;
    r.measured = std::to_string (moved) + " internal parameters moved, " + std::to_string (allocations) + " allocations; side/mid "
                 + fmtDb (lo) + " at Width 10 %, " + fmtDb (hi) + " at 90 %";
    return r;
}

Result p3t9Classifier()
{
    Result r { "P3-T9", "Classifier after 8 s: drum loop percussive > 0.6; melody tonal > 0.6; mix mixed > 0.4; weights move by at most 0.05 per 100 ms (drum loop, melody, mix in sequence)", "", false, true, "" };
    const int n = samples (8.0, fs);
    const Signal seq[3] = { tile (drumLoop (fs), n), tile (melody (fs), n), tile (mix (fs), n) };
    MaterialClassifier c;
    c.prepare (fs);
    const int step = samples (0.1, fs);
    double maxMove = 0.0;
    float end[3][3] {};
    for (int s = 0; s < 3; ++s)
    {
        for (int pos = 0; pos + step <= n; pos += step)
        {
            const auto before = c.all();
            c.process (seq[s].data() + pos, step);
            for (int k = 0; k < 3; ++k)
                maxMove = std::max (maxMove, (double) std::abs (c.weight (k) - before[(size_t) k]));
        }
        for (int k = 0; k < 3; ++k)
            end[s][k] = c.weight (k);
    }
    // Each material alone, from a fresh classifier.
    float alone[3] {};
    for (int s = 0; s < 3; ++s)
    {
        MaterialClassifier a;
        a.prepare (fs);
        a.process (seq[s].data(), n);
        alone[s] = a.weight (s == 0 ? MaterialClassifier::percussive : (s == 1 ? MaterialClassifier::tonal : MaterialClassifier::mixed));
    }
    r.pass = alone[0] > 0.6f && alone[1] > 0.6f && alone[2] > 0.4f && maxMove <= 0.05;
    r.measured = "drum loop percussive " + fmt (alone[0], 2) + ", melody tonal " + fmt (alone[1], 2) + ", mix mixed " + fmt (alone[2], 2)
                 + " (in sequence " + fmt (end[0][0], 2) + ", " + fmt (end[1][1], 2) + ", " + fmt (end[2][2], 2) + "); largest move "
                 + fmt (maxMove, 3) + " per 100 ms";
    return r;
}

Result p3t10EasyCost()
{
    Result r { "P3-T10", "Easy mode at defaults on 60 s of mix: <= 4 % of real time (median of 3)", "", false, true, "" };
    const Signal x = tile (mix (fs), samples (60.0, fs));
    std::vector<double> runs;
    for (int run = 0; run < 3; ++run)
    {
        Plugin pl (fs);
        easytest::applyMacros (pl, easy::Macros {});
        pl.prepare();
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        pl.render (x);
        runs.push_back ((juce::Time::getMillisecondCounterHiRes() - t0) * 0.001);
    }
    std::sort (runs.begin(), runs.end());
    const double pct = 100.0 * runs[1] / 60.0;
    r.pass = pct <= 4.0;
    r.measured = fmt (pct, 2) + " % of real time";
    return r;
}

Result p3t12SessionCompatibility()
{
    Result r { "P3-T12", "Sessions saved by 1.0.0 and 2.0.0 open in Complete mode and render bit-identically (20 + 20 fixtures)", "", true, false, "" };
    int same = 0, complete = 0, total = 0;
    for (const char* sub : { "", "v2" })
    {
        const auto dir = juce::File (SPH_SOURCE_DIR).getChildFile ("tests/fixtures").getChildFile (sub);
        juce::StringArray lines;
        lines.addLines (dir.getChildFile ("hashes.txt").loadFileAsString());
        lines.removeEmptyStrings();
        for (const auto& line : lines)
        {
            const auto name = line.upToFirstOccurrenceOf (" ", false, false);
            const auto expected = line.fromFirstOccurrenceOf (" ", false, false).trim().toStdString();
            juce::MemoryBlock state;
            if (! dir.getChildFile (name + ".state").loadFileAsData (state))
                continue;
            Plugin pl (fs);
            pl.set (ids::ui_mode, 0); // a fresh instance is in Easy mode
            pl.processor().setStateInformation (state.getData(), (int) state.getSize());
            complete += pl.processor().isEasyMode() ? 0 : 1;
            pl.prepare();
            same += hashRender (pl.render (mix (fs))) == expected ? 1 : 0;
            ++total;
        }
    }
    r.pass = total == 40 && same == 40 && complete == 40;
    r.measured = std::to_string (same) + "/" + std::to_string (total) + " bit-identical, " + std::to_string (complete) + " in Complete mode";
    return r;
}

Result p3t11EasySnapshots()
{
    Result r { "P3-T11", "Snapshots of Easy mode in English and Czech at 1 x and 1.5 x zoom, and of the collapse question, exist and have been inspected (docs/ui-easy/)", "", true, false, "" };
    const auto dir = juce::File (SPH_SOURCE_DIR).getChildFile ("docs/ui-easy");
    dir.createDirectory();
    int written = 0;
    for (int language = 0; language < 2; ++language)
        for (int shot = 0; shot < 3; ++shot)
        {
            Plugin pl (fs);
            easytest::applyMacros (pl, easy::Macros {});
            if (shot == 2)
                pl.set (ids::ui_mode, 1);
            pl.prepare();
            pl.processor().setTeachingSource (5);
            auto editor = std::unique_ptr<juce::AudioProcessorEditor> (pl.processor().createEditor());
            auto* ed = dynamic_cast<PluginEditor*> (editor.get());
            const float zoom = shot == 1 ? 1.5f : 1.0f;
            ed->setZoom (zoom);
            ed->setInfoVisible (true);
            ed->setLanguageForTest (language);
            ed->hoverForTest (shot == 2 ? "ui.collapse" : "p.easy_width");
            const Signal silence ((size_t) samples (0.5, fs), 0.0f);
            for (int part = 0; part < 8; ++part)
            {
                pl.render (silence); // the drum loop teaching source replaces it
                ed->refreshForTest();
            }
            if (shot == 2)
                ed->pressModeButtonForTest();
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            const juce::String name = shot == 2 ? "collapse" : (shot == 1 ? "easy-150" : "easy");
            const auto file = dir.getChildFile (name + (language == 0 ? "-en" : "-cz") + ".png");
            file.deleteFile();
            if (auto out = file.createOutputStream())
                if (juce::PNGImageFormat().writeImageToStream (image, *out)
                    && image.getWidth() == (int) std::lround (PluginEditor::fullWidth * zoom))
                    ++written;
            ed->setLanguageForTest (0);
            pl.processor().setTeachingSource (0);
        }
    r.pass = written == 6;
    r.measured = std::to_string (written) + " of 6 snapshots written; inspected, see docs/DECISIONS.md";
    return r;
}
} // namespace sph::measure
