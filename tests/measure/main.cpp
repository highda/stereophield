// sph_measure: runs every measurement of DESIGN.md section 10, rewrites
// docs/MEASUREMENTS.md and renders each factory preset on the mix signal.
//
//   sph_measure --all --out docs/MEASUREMENTS.md --renders build/renders
//   sph_measure T2 T13            (selected tests, printed only)

#include "common/Registry.h"
#include "common/Render.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdio>
#include <map>

using namespace sph;

namespace
{
juce::String escape (const std::string& s)
{
    return juce::String (s).replace ("|", "\\|");
}

void renderPresets (const juce::File& dir)
{
    dir.createDirectory();
    const auto& presets = factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        test::Plugin pl;
        pl.preset (i + 1);
        pl.prepare();
        const auto x = signals::mix (pl.fs);
        const auto y = pl.render (x);

        juce::String name = juce::String (i + 1).paddedLeft ('0', 2) + " " + presets[(size_t) i].name;
        name = name.replaceCharacters (":,", "--").replace (" ", "_");
        const auto file = dir.getChildFile (name + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        auto stream = file.createOutputStream();
        if (stream == nullptr)
            continue;
        std::unique_ptr<juce::OutputStream> os (stream.release());
        auto writer = wav.createWriterFor (os, juce::AudioFormatWriterOptions {}
                                                   .withSampleRate (pl.fs)
                                                   .withNumChannels (2)
                                                   .withBitsPerSample (24));
        if (writer == nullptr)
            continue;
        const float* chans[] = { y.l.data(), y.r.data() };
        writer->writeFromFloatArrays (chans, 2, (int) y.l.size());

        // Width summary after the 1 s warm-up: side-to-mid energy and
        // left-right correlation.
        double mm = 0, ss = 0, lr = 0, ll = 0, rr = 0;
        for (size_t k = (size_t) pl.fs; k < y.l.size(); ++k)
        {
            const double l = y.l[k], r = y.r[k];
            mm += 0.25 * (l + r) * (l + r);
            ss += 0.25 * (l - r) * (l - r);
            lr += l * r;
            ll += l * l;
            rr += r * r;
        }
        std::printf ("rendered %-44s side/mid %6.1f dB, correlation %5.2f\n", file.getFileName().toRawUTF8(),
                     10.0 * std::log10 (ss / mm + 1e-30), lr / std::sqrt (ll * rr + 1e-30));
    }
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    bool all = false;
    juce::String outPath, rendersPath;
    std::vector<juce::String> selected;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--all")
            all = true;
        else if (a == "--out" && i + 1 < argc)
            outPath = argv[++i];
        else if (a == "--renders" && i + 1 < argc)
            rendersPath = argv[++i];
        else
            selected.push_back (a);
    }

    std::vector<measure::Result> results;
    for (const auto& e : measure::registry())
    {
        if (! all && std::find (selected.begin(), selected.end(), juce::String (e.id)) == selected.end())
            continue;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto r = e.fn();
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::printf ("%-4s %-8s %s  (%.1f s)\n", r.id.c_str(),
                     r.pass ? "pass" : (r.tunable && ! r.note.empty() ? "fallback" : "FAIL"),
                     r.measured.c_str(), ms / 1000.0);
        std::fflush (stdout);
        results.push_back (r);
    }

    if (outPath.isNotEmpty())
    {
        juce::String md;
        md << "# Measurements\n\n"
           << "Written by `sph_measure --all` on " << juce::Time::getCurrentTime().formatted ("%Y-%m-%d") << ". "
           << "Release build, 48 kHz and block 512 unless a criterion says otherwise.\n\n"
           << "| Test | Criterion | Measured | Result |\n| --- | --- | --- | --- |\n";
        for (const auto& r : results)
        {
            juce::String verdict = r.pass ? "pass" : (r.tunable && ! r.note.empty() ? "fallback" : "**fail**");
            if (! r.note.empty())
                verdict << " (" << escape (r.note) << ")";
            md << "| " << r.id << " | " << escape (r.criterion) << " | " << escape (r.measured) << " | " << verdict << " |\n";
        }
        juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (outPath)).replaceWithText (md);
        std::printf ("wrote %s\n", outPath.toRawUTF8());
    }

    if (rendersPath.isNotEmpty())
        renderPresets (juce::File::getCurrentWorkingDirectory().getChildFile (rendersPath));

    int failures = 0;
    for (const auto& r : results)
        failures += (r.pass || (r.tunable && ! r.note.empty())) ? 0 : 1;
    return failures == 0 ? 0 : 1;
}
