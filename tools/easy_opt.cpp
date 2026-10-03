// sph_easy_opt: optimises the Easy mode table (PART3_LEDGER.md section 4.2)
// and writes src/plugin/EasyTables.h. Offline; never shipped.
//
//   sph_easy_opt --probe                     classifier and metrics of the current table
//   sph_easy_opt --metric <item> W C S F low  metrics of one setting
//   sph_easy_opt --optimise [--references <folder> | --target <ASW at Width 100 %>] [--write]
//
// The optimisation, per latency mode: every corpus item is rendered with its
// own classifier weights (their mean over the measured part), over a grid of
// the effective side gain x (the `width` parameter, 0 .. 200 %) and of
// Character. Every metric is then a measured function of x at each Character
// point. An item's x under a candidate table is the class blend of
// width(W) x gain(C), as in the plugin. Coordinate descent over the control
// points of all three classes minimises J of section 4.2 on these functions
// (interpolated in x); --probe verifies the result with real renders.

#include "common/EasyMetrics.h"
#include "dsp/VirtualListener.h"
#include "plugin/EasyTables.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdio>
#include <future>
#include <thread>

using namespace sph;

namespace
{
constexpr double fs = 48000.0;
constexpr double seconds = 4.0; // as the P3 tests
constexpr int numX = 21;          // x = 0, 10 .. 200 %
constexpr int numC = 5;           // Character 0, 25 .. 100 %
constexpr float focusDefault = 0.5f, spaceDefault = 0.2f;

float xAt (int i) { return 10.0f * (float) i; }
float cAt (int j) { return 0.25f * (float) j; }

// Tone and click (a pure sine has no measurable width) only enters the
// constraints.
bool countsForWidth (const std::string& name) { return name != "tone and click"; }

std::vector<std::thread>::size_type threads() { return std::max (1u, std::thread::hardware_concurrency()); }

void probe()
{
    const auto items = corpus::items (fs, 6.0);
    std::printf ("%-16s %6s %6s %6s  %s\n", "item", "perc", "tonal", "mixed", "onset/s, tonal share");
    for (const auto& item : items)
    {
        MaterialClassifier c;
        c.prepare (fs);
        c.process (item.x.data(), (int) item.x.size());
        std::printf ("%-16s %6.2f %6.2f %6.2f   %5.2f %5.2f\n", item.name.c_str(), c.weight (0), c.weight (1), c.weight (2),
                     c.percussiveOnsetRate(), c.tonalShare());
    }
    std::printf ("\n%-16s %5s %6s %6s %7s %7s %8s\n", "item", "W", "ASW", "minCor", "dLUFS", "bassS", "trans");
    for (const auto& item : items)
        for (float w : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            easytest::Setting s;
            s.macros.width = w;
            const auto r = easytest::render (item.x, s, fs);
            const auto m = easytest::measure (item.x, r, fs, easytest::hits (item.name, item.x.size(), fs));
            std::printf ("%-16s %5.2f %6.3f %6.2f %7.2f %7.1f %8.1f\n", item.name.c_str(), w, m.asw, m.minCorrelation, m.lufsChange,
                         m.bassSideDb, m.transientDb);
        }
}

void metric (const char* which, float w, float c, float sp, float f, bool low)
{
    for (const auto& item : corpus::items (fs, 6.0))
        if (item.name == which)
        {
            easytest::Setting s;
            s.macros = { w, c, sp, f, true, low };
            const auto r = easytest::render (item.x, s, fs);
            const auto m = easytest::measure (item.x, r, fs, easytest::hits (item.name, item.x.size(), fs));
            std::printf ("%-16s W %.2f C %.2f S %.2f F %.2f %s: ASW %.3f minCor %.2f dLUFS %.2f bass %.1f trans %.1f\n", which, w, c,
                         sp, f, low ? "low" : "full", m.asw, m.minCorrelation, m.lufsChange, m.bassSideDb, m.transientDb);
        }
}

// ---------------------------------------------------------------- response grid

struct Response
{
    std::string item;
    bool hits = false;
    easy::Weights weights {}; // the classifier's mean weights over the measured part
    easytest::Metrics m[numC][numX];
    double reach = 0.0;       // the widest ASW within the constraints, Character 50 %
};

// The widest ASW an item reaches at Character 50 % without breaking a
// constraint, a little below the peak so that the curve need not sit on it.
double reachable (const Response& r)
{
    double best = 0.0;
    for (int x = 0; x < numX; ++x)
    {
        const auto& m = r.m[2][x];
        if (m.minCorrelation >= 0.0 && m.lufsChange <= 1.5 && m.bassSideDb <= -12.0)
            best = std::max (best, m.asw);
    }
    return 0.95 * best;
}

// The classifier's mean weights from 1 s to the end of an item, as the
// verification renders measure them.
easy::Weights meanWeights (const signals::Signal& x)
{
    MaterialClassifier c;
    c.prepare (fs);
    easy::Weights sum {};
    int count = 0;
    const int block = 512;
    for (int pos = 0; pos + block <= (int) x.size(); pos += block)
    {
        c.process (x.data() + pos, block);
        if (pos >= (int) fs)
        {
            for (int k = 0; k < easy::numClasses; ++k)
                sum[(size_t) k] += c.weight (k);
            ++count;
        }
    }
    for (auto& w : sum)
        w /= (float) std::max (1, count);
    return sum;
}

// Metrics of every corpus item over the (Character, x) grid for one latency
// mode: every class's width curve is the constant x, so the blend is x, and
// the generator amounts are those of the item's own blend of classes.
std::vector<Response> measureLatency (int lat, const std::vector<corpus::Item>& items)
{
    std::vector<Response> out (items.size());
    for (size_t i = 0; i < items.size(); ++i)
    {
        out[i].item = items[i].name;
        out[i].weights = meanWeights (items[i].x);
        out[i].hits = ! easytest::hits (items[i].name, items[i].x.size(), fs).empty();
    }
    const size_t jobs = items.size() * numC * numX;
    std::atomic<size_t> next { 0 };
    auto worker = [&]
    {
        for (size_t j; (j = next.fetch_add (1)) < jobs;)
        {
            const size_t i = j / (numC * numX);
            const int c = (int) (j / numX % numC), x = (int) (j % numX);
            easy::Table t = easy::table();
            for (auto& ct : t.cls[lat])
                for (int p = 0; p < easy::numPoints; ++p)
                {
                    ct.width.y[p] = xAt (x);
                    ct.charGain.y[p] = 1.0f;
                }
            easytest::Setting s;
            s.macros = { 1.0f, cAt (c), spaceDefault, focusDefault, true, lat == 1 };
            s.table = &t;
            s.weights = &out[i].weights;
            const auto r = easytest::render (items[i].x, s, fs);
            out[i].m[c][x] = easytest::measure (items[i].x, r, fs, easytest::hits (items[i].name, items[i].x.size(), fs));
        }
    };
    std::vector<std::thread> pool;
    for (size_t t = 0; t < threads(); ++t)
        pool.emplace_back (worker);
    for (auto& t : pool)
        t.join();
    for (auto& r : out)
        r.reach = reachable (r);
    return out;
}

// A metric of one response at Character grid point c and gain x, linear in x.
template <typename F> double lookup (const Response& r, int c, float x, F get)
{
    const float t = std::clamp (x / 10.0f, 0.0f, (float) (numX - 1));
    const int i = std::min ((int) t, numX - 2);
    const double f = t - (float) i;
    return (1.0 - f) * get (r.m[c][i]) + f * get (r.m[c][i + 1]);
}

// The curves of the three classes of one latency mode.
struct Candidate
{
    float width[easy::numClasses][easy::numPoints];
    float gain[easy::numClasses][easy::numPoints];
};

void normalise (Candidate& c)
{
    for (int k = 0; k < easy::numClasses; ++k)
    {
        c.width[k][0] = 0.0f;
        for (int p = 1; p < easy::numPoints; ++p)
            c.width[k][p] = std::clamp (c.width[k][p], c.width[k][p - 1], 200.0f);
        for (auto& g : c.gain[k])
            g = std::clamp (g, 0.3f, 3.0f);
    }
}

struct Terms
{
    double width = 0, rising = 0, correlation = 0, loudness = 0, bass = 0, transients = 0, smooth = 0;
    // Transient protection is set by Focus and Character, not by the curves
    // optimised here, so it is reported but not minimised (P3-T5 checks it).
    double total() const { return width + rising + correlation + loudness + bass + smooth; }
};

// The side gain the mapping gives an item: the class blend of width x gain.
float effectiveX (const Candidate& cand, const easy::Weights& w, float W, float C)
{
    float x = 0.0f;
    for (int k = 0; k < easy::numClasses; ++k)
    {
        easy::Curve wc, gc;
        std::copy (cand.width[k], cand.width[k] + easy::numPoints, wc.y);
        std::copy (cand.gain[k], cand.gain[k] + easy::numPoints, gc.y);
        x += w[(size_t) k] * wc.at (W) * gc.at (C);
    }
    return std::clamp (x, 0.0f, 200.0f);
}

Terms objective (const Candidate& cand, const std::vector<Response>& resp, double aswMax)
{
    Terms t;
    for (const auto& r : resp)
        for (int c = 0; c < numC; ++c)
            for (int wi = 0; wi <= 10; ++wi)
            {
                const float W = 0.1f * (float) wi;
                const float x = effectiveX (cand, r.weights, W, cAt (c));
                if (countsForWidth (r.item) && wi > 0)
                {
                    // Every step of Width must widen (P3-T2): at least 0.01.
                    const float xPrev = effectiveX (cand, r.weights, W - 0.1f, cAt (c));
                    const double rise = lookup (r, c, x, [] (const auto& m) { return m.asw; })
                                        - lookup (r, c, xPrev, [] (const auto& m) { return m.asw; });
                    t.rising += 1000.0 * std::pow (std::max (0.0, 0.01 - rise), 2.0);
                }
                if (countsForWidth (r.item))
                {
                    // Linear up to what the item can reach (drums, kept
                    // centred on their hits, stay narrower).
                    const double e = lookup (r, c, x, [] (const auto& m) { return m.asw; }) - W * std::min (aswMax, r.reach);
                    t.width += 100.0 * e * e;
                }
                const double cor = lookup (r, c, x, [] (const auto& m) { return m.minCorrelation; });
                const double lu = lookup (r, c, x, [] (const auto& m) { return m.lufsChange; });
                const double bass = lookup (r, c, x, [] (const auto& m) { return m.bassSideDb; });
                t.correlation += 1000.0 * std::pow (std::max (0.0, -cor), 2.0);
                t.loudness += 50.0 * std::pow (std::max (0.0, lu - 1.5), 2.0);
                t.bass += 50.0 * std::pow (std::max (0.0, bass + 12.0), 2.0);
                if (r.hits)
                {
                    const double tr = lookup (r, c, x, [] (const auto& m) { return std::min (m.transientDb, 30.0); });
                    t.transients += 10.0 * std::pow (std::max (0.0, 6.0 - tr), 2.0);
                }
            }
    for (int k = 0; k < easy::numClasses; ++k)
        for (int p = 1; p + 1 < easy::numPoints; ++p)
        {
            t.smooth += std::pow ((cand.width[k][p - 1] - 2 * cand.width[k][p] + cand.width[k][p + 1]) / 100.0, 2.0);
            t.smooth += std::pow (cand.gain[k][p - 1] - 2 * cand.gain[k][p] + cand.gain[k][p + 1], 2.0);
        }
    return t;
}

Candidate descend (Candidate c, const std::vector<Response>& resp, double aswMax)
{
    normalise (c);
    double best = objective (c, resp, aswMax).total();
    float stepW = 40.0f, stepG = 0.4f;
    for (int round = 0; round < 200 && stepW > 0.05f; ++round)
    {
        bool improved = false;
        for (int k = 0; k < easy::numClasses; ++k)
            for (int v = 1; v < 2 * easy::numPoints; ++v)
                for (float dir : { 1.0f, -1.0f })
                {
                    Candidate t = c;
                    if (v < easy::numPoints)
                        t.width[k][v] += dir * stepW;
                    else
                        t.gain[k][v - easy::numPoints] += dir * stepG;
                    normalise (t);
                    const double j = objective (t, resp, aswMax).total();
                    if (j < best - 1e-9)
                    {
                        best = j;
                        c = t;
                        improved = true;
                    }
                }
        if (! improved)
        {
            stepW *= 0.5f;
            stepG *= 0.5f;
        }
    }
    return c;
}

// A (1 + 16) evolution strategy over every control point at once, with the
// one-fifth success rule for the step size; it escapes the narrow valleys
// that the rising constraint makes for coordinate descent.
Candidate evolve (Candidate c, const std::vector<Response>& resp, double aswMax, uint32_t seed)
{
    normalise (c);
    double best = objective (c, resp, aswMax).total();
    Rng rng (seed);
    double sigma = 1.0; // in units of 20 % width and 0.2 gain
    for (int gen = 0; gen < 4000 && sigma > 1e-3; ++gen)
    {
        int successes = 0;
        Candidate bestChild = c;
        double bestJ = best;
        for (int child = 0; child < 16; ++child)
        {
            Candidate t = c;
            for (int k = 0; k < easy::numClasses; ++k)
                for (int p = 0; p < easy::numPoints; ++p)
                {
                    t.width[k][p] += (float) (20.0 * sigma * rng.gaussian());
                    t.gain[k][p] += (float) (0.2 * sigma * rng.gaussian());
                }
            normalise (t);
            const double j = objective (t, resp, aswMax).total();
            if (j < best)
                ++successes;
            if (j < bestJ)
            {
                bestJ = j;
                bestChild = t;
            }
        }
        if (bestJ < best)
        {
            best = bestJ;
            c = bestChild;
        }
        sigma *= successes * 5 > 16 ? 1.2 : 0.85;
    }
    return c;
}

// ---------------------------------------------------------------- references

double referenceAswMax (const juce::File& folder)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::vector<double> widths;
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff"))
    {
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
        if (reader == nullptr || reader->numChannels < 2)
            continue;
        const int n = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));
        juce::AudioBuffer<float> buf (2, n);
        reader->read (&buf, 0, n, 0, true, true);
        VirtualListener vl;
        vl.prepare (reader->sampleRate);
        const int win = vl.windowSize();
        double sum = 0;
        int count = 0;
        for (int s = 0; s + win <= n; s += win)
        {
            vl.addWindow (buf.getReadPointer (0, s), buf.getReadPointer (1, s));
            if (s >= 3 * win)
            {
                sum += vl.asw();
                ++count;
            }
        }
        if (count > 0)
        {
            widths.push_back (sum / count);
            std::printf ("reference %-40s ASW %.3f\n", f.getFileName().toRawUTF8(), sum / count);
        }
    }
    if (widths.empty())
        return 0.45;
    std::sort (widths.begin(), widths.end());
    return std::clamp (2.0 * widths[widths.size() / 2], 0.2, 0.55);
}

// ---------------------------------------------------------------- output

juce::String fmtCurve (const float* y)
{
    juce::String s = "{ { ";
    for (int p = 0; p < easy::numPoints; ++p)
        s << juce::String (y[p], 3).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") << (p + 1 < easy::numPoints ? ", " : " } }");
    return s;
}

juce::String fmtNum (float v) { return juce::String (v, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."); }

void writeTable (const easy::Table& t, double aswMax, const juce::String& summary)
{
    static const char* names[] = { "percussive", "tonal", "mixed" };
    juce::String s;
    s << "#pragma once\n\n"
      << "// Easy mode table, written by sph_easy_opt --optimise --write (PART3_LEDGER.md\n"
      << "// section 4.2). Do not edit by hand: change tools/easy_opt.cpp and rerun.\n"
      << "// Target: ASW = Width x min (" << juce::String (aswMax, 3) << ", the item's reachable width), loudspeaker listener.\n";
    juce::StringArray lines;
    lines.addLines (summary);
    lines.removeEmptyStrings();
    for (const auto& l : lines)
        s << "// " << l << "\n";
    s << "\n#include \"plugin/EasyMode.h\"\n\nnamespace sph::easy\n{\n"
      << "// Per class: width curve, Character gain curve, then the amounts of\n"
      << "// spread, coherence (clean), velvet, coherence (diffuse), double, mod, pan.\n"
      << "inline constexpr Table generatedTable { {\n";
    for (int lat = 0; lat < 2; ++lat)
    {
        s << "    { // " << (lat == 0 ? "Full engine" : "Low latency (Light engine): no spectral generators") << "\n";
        for (int k = 0; k < easy::numClasses; ++k)
        {
            const auto& c = t.cls[lat][k];
            s << "      { " << fmtCurve (c.width.y) << ", " << fmtCurve (c.charGain.y) << ", " << fmtNum (c.spread) << ", "
              << fmtNum (c.cohPhase) << ", " << fmtNum (c.velvet) << ", " << fmtNum (c.coh) << ", " << fmtNum (c.dbl) << ", "
              << fmtNum (c.mod) << ", " << fmtNum (c.pan) << " }" << (k + 1 < easy::numClasses ? "," : " }") << " // " << names[k]
              << "\n";
        }
        s << (lat == 0 ? "    ,\n" : "");
    }
    s << "} };\n} // namespace sph::easy\n";
    s = s.replace ("}\n    ,\n", "},\n");
    const auto file = juce::File (SPH_SOURCE_DIR).getChildFile ("src/plugin/EasyTables.h");
    file.replaceWithText (s);
    std::printf ("wrote %s\n", file.getFullPathName().toRawUTF8());
}

void optimise (double aswMax, bool write)
{
    const auto items = corpus::items (fs, seconds);
    easy::Table result = easy::table();
    juce::String summary;
    for (int lat = 0; lat < 2; ++lat)
    {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const auto resp = measureLatency (lat, items);
        for (const auto& r : resp)
            std::printf ("   %-16s weights %.2f %.2f %.2f, reachable ASW %.3f\n", r.item.c_str(), r.weights[0], r.weights[1], r.weights[2],
                         r.reach);
        // From the hand-written start (linear width, even gain), so that a
        // rerun reproduces the table; the generator amounts are kept.
        Candidate start;
        for (int k = 0; k < easy::numClasses; ++k)
            for (int p = 0; p < easy::numPoints; ++p)
            {
                start.width[k][p] = 25.0f * (float) p;
                start.gain[k][p] = 1.0f;
            }
        const auto before = objective (start, resp, aswMax);
        // Evolution and descent from the start and from 24 random starts
        // (fixed seeds); the surrogate is cheap, the response grid is
        // measured once.
        auto best = descend (evolve (start, resp, aswMax, 77u + (uint32_t) lat), resp, aswMax);
        Rng rng (2026u + (uint32_t) lat);
        for (int restart = 0; restart < 24; ++restart)
        {
            Candidate c;
            for (int k = 0; k < easy::numClasses; ++k)
                for (int p = 0; p < easy::numPoints; ++p)
                {
                    c.width[k][p] = (float) (rng.uniform() * 200.0);
                    c.gain[k][p] = (float) (0.5 + rng.uniform() * 1.5);
                }
            for (int k = 0; k < easy::numClasses; ++k)
                std::sort (c.width[k], c.width[k] + easy::numPoints);
            const auto d = descend (evolve (c, resp, aswMax, 1000u + (uint32_t) restart), resp, aswMax);
            if (objective (d, resp, aswMax).total() < objective (best, resp, aswMax).total())
                best = d;
        }
        const auto after = objective (best, resp, aswMax);
        for (int k = 0; k < easy::numClasses; ++k)
        {
            std::copy (best.width[k], best.width[k] + easy::numPoints, result.cls[lat][k].width.y);
            std::copy (best.gain[k], best.gain[k] + easy::numPoints, result.cls[lat][k].charGain.y);
        }
        const auto line = juce::String::formatted (
            "%s: J %.2f -> %.2f (width %.2f, rising %.2f; correlation %.2f, loudness %.2f, bass %.2f; smoothness %.3f; transients, not minimised, %.2f)",
            lat == 0 ? "Full" : "Low latency", before.total(), after.total(), after.width, after.rising, after.correlation, after.loudness, after.bass,
            after.smooth, after.transients);
        std::printf ("%s  [%.0f s]\n", line.toRawUTF8(), (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001);
        // Predicted ASW per item at Character 50 %.
        for (const auto& r : resp)
        {
            std::printf ("   %-16s", r.item.c_str());
            for (int wi = 0; wi <= 10; wi += 2)
                std::printf (" %.2f", lookup (r, 2, effectiveX (best, r.weights, 0.1f * (float) wi, 0.5f), [] (const auto& m) { return m.asw; }));
            std::printf ("\n");
        }
        summary << line << "\n";
        std::fflush (stdout);
    }
    if (write)
        writeTable (result, aswMax, summary);
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    double aswMax = 0.45;
    bool write = false, run = false;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--probe")
            probe();
        else if (a == "--metric" && i + 6 < argc)
        {
            metric (argv[i + 1], (float) std::atof (argv[i + 2]), (float) std::atof (argv[i + 3]), (float) std::atof (argv[i + 4]),
                    (float) std::atof (argv[i + 5]), std::atoi (argv[i + 6]) != 0);
            i += 6;
        }
        else if (a == "--references" && i + 1 < argc)
            aswMax = referenceAswMax (juce::File::getCurrentWorkingDirectory().getChildFile (argv[++i]));
        else if (a == "--target" && i + 1 < argc)
            aswMax = std::atof (argv[++i]);
        else if (a == "--write")
            write = true;
        else if (a == "--optimise")
            run = true;
    }
    if (run)
        optimise (aswMax, write);
    return 0;
}
