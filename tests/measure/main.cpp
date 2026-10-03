// sph_measure: runs every measurement of DESIGN.md section 10, rewrites
// docs/MEASUREMENTS.md and renders each factory preset on the mix signal.
//
//   sph_measure --all --out docs/MEASUREMENTS.md --renders build/renders
//   sph_measure T2 T13            (selected tests, printed only)

#include "common/Registry.h"
#include "common/Render.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include "common/Fixtures.h"
#include "common/MeasurePlugin.h"
#include "dsp/StereoBands.h"
#include "ui/Strings.h"
#include "ui/Strings.h"

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
// Session fixtures for P2-T30: random states saved by this build, with the
// SHA-256 of their rendered output.
void makeFixtures (const juce::File& dir)
{
    dir.createDirectory();
    juce::String hashes;
    for (int i = 0; i < 20; ++i)
    {
        test::Plugin pl;
        Rng rng (20000u + (uint32_t) i);
        for (const char* id : ids::all)
            pl.setNormalised (id, (float) rng.uniform());
        juce::MemoryBlock state;
        pl.processor().getStateInformation (state);
        const auto name = "session_" + juce::String (i).paddedLeft ('0', 2);
        dir.getChildFile (name + ".state").replaceWithData (state.getData(), state.getSize());

        test::Plugin fresh;
        fresh.processor().setStateInformation (state.getData(), (int) state.getSize());
        fresh.prepare();
        const auto y = fresh.render (signals::mix (fresh.fs));
        hashes << name << " " << test::hashRender (y) << "\n";
    }
    dir.getChildFile ("hashes.txt").replaceWithText (hashes);
    std::printf ("wrote 20 fixtures to %s\n", dir.getFullPathName().toRawUTF8());
}
bool writeWav (const juce::File& file, const signals::Signal& l, const signals::Signal& r, double fs)
{
    file.deleteFile();
    juce::WavAudioFormat wav;
    auto stream = file.createOutputStream();
    if (stream == nullptr)
        return false;
    std::unique_ptr<juce::OutputStream> os (stream.release());
    auto writer = wav.createWriterFor (os, juce::AudioFormatWriterOptions {}.withSampleRate (fs).withNumChannels (2).withBitsPerSample (24));
    if (writer == nullptr)
        return false;
    const float* chans[] = { l.data(), r.data() };
    return writer->writeFromFloatArrays (chans, 2, (int) l.size());
}

void writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    if (auto out = f.createOutputStream())
        juce::PNGImageFormat().writeImageToStream (img, *out);
}

juce::Image goniometerImage (const signals::Signal& l, const signals::Signal& r)
{
    juce::Image img (juce::Image::RGB, 200, 200, true);
    juce::Graphics g (img);
    g.fillAll (juce::Colour (0xff14161a));
    g.setColour (juce::Colour (0xff2c3038));
    g.drawLine (100, 0, 100, 200);
    g.drawLine (0, 100, 200, 100);
    g.setColour (juce::Colour (0xff4fd1c5).withAlpha (0.25f));
    for (size_t i = 0; i < l.size(); i += 3)
    {
        const float x = 100.0f + (r[i] - l[i]) * 0.7071f * 90.0f, y = 100.0f - (l[i] + r[i]) * 0.7071f * 90.0f;
        g.fillRect (x, y, 1.2f, 1.2f);
    }
    return img;
}

juce::Image bandsImage (const signals::Signal& l, const signals::Signal& r, const signals::Signal& m, double fs)
{
    StereoBands sb;
    sb.prepare (fs, 10.0);
    for (size_t s0 = (size_t) fs; s0 + StereoBands::frameSize <= l.size(); s0 += StereoBands::frameSize / 2)
        sb.addFrame (l.data() + s0, r.data() + s0, m.data() + s0);
    juce::Image img (juce::Image::RGB, 300, 100, true);
    juce::Graphics g (img);
    g.fillAll (juce::Colour (0xff14161a));
    g.setColour (juce::Colour (0xff2c3038));
    g.drawLine (0, 50, 300, 50);
    const float w = 300.0f / StereoBands::numBands;
    for (int b = 0; b < StereoBands::numBands; ++b)
    {
        const float c = juce::jlimit (-1.0f, 1.0f, sb.correlation (b));
        g.setColour (c < 0 ? juce::Colour (0xfff56565) : juce::Colour (0xff4fd1c5));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (b * w + 1, std::min (50.0f, 50.0f - c * 46.0f), (b + 1) * w - 1, std::max (50.0f, 50.0f - c * 46.0f)));
    }
    return img;
}

juce::String html (const juce::String& s) { return s.replace ("&", "&amp;").replace ("<", "&lt;").replace (">", "&gt;"); }

// The listening report of PART2_LEDGER.md I13.
void writeReport (const juce::File& dir)
{
    dir.createDirectory();
    const double fs = 48000.0;
    const auto x = signals::tile (signals::mix (fs), signals::samples (6.0, fs));
    writeWav (dir.getChildFile ("dry.wav"), x, x, fs);
    const auto& presets = factoryPresets();
    juce::String rows;
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        test::Plugin pl;
        pl.preset (i + 1);
        pl.prepare();
        const auto y = pl.render (x);
        signals::Signal mono (y.l.size()), ref (y.l.size(), 0.0f);
        for (size_t k = 0; k < mono.size(); ++k)
            mono[k] = 0.5f * (y.l[k] + y.r[k]);
        for (size_t k = (size_t) pl.latency(); k < ref.size(); ++k)
            ref[k] = x[k - (size_t) pl.latency()];
        const auto name = juce::String (i + 1).paddedLeft ('0', 2);
        writeWav (dir.getChildFile (name + ".wav"), y.l, y.r, fs);
        writeWav (dir.getChildFile (name + "-mono.wav"), mono, mono, fs);
        writePng (goniometerImage (y.l, y.r), dir.getChildFile (name + "-gonio.png"));
        writePng (bandsImage (y.l, y.r, ref, fs), dir.getChildFile (name + "-bands.png"));
        const auto m = measure::perceptual (x, y.l, y.r, pl.latency(), fs);
        ui::Help en, cs;
        ui::helpIn ("preset." + juce::String (i + 1), ui::Language::English, en);
        ui::helpIn ("preset." + juce::String (i + 1), ui::Language::Czech, cs);
        rows << "<section class=\"preset\"><h2>" << (i + 1) << ". " << html (presets[(size_t) i].name) << "</h2>"
             << "<p class=\"desc\">" << html (en.what) << "<br><span class=\"cz\">" << html (cs.title) << ": " << html (cs.what) << "</span></p>"
             << "<div class=\"row\"><div class=\"players\">"
             << "<label>Dry <audio controls preload=\"none\" src=\"dry.wav\"></audio></label>"
             << "<label>Processed <audio controls preload=\"none\" src=\"" << name << ".wav\"></audio></label>"
             << "<label>Mono fold <audio controls preload=\"none\" src=\"" << name << "-mono.wav\"></audio></label></div>"
             << "<img src=\"" << name << "-gonio.png\" alt=\"goniometer\"><img src=\"" << name << "-bands.png\" alt=\"correlation per band\">"
             << "<table><tr><td>Correlation</td><td>" << measure::fmt (m.correlation, 2) << "</td></tr>"
             << "<tr><td>Perceived width</td><td>" << measure::fmt (m.asw, 2) << "</td></tr>"
             << "<tr><td>Mono fold</td><td>" << measure::fmt (m.monoFoldDb, 1) << " dB</td></tr>"
             << "<tr><td>Loudness</td><td>" << measure::fmt (m.lufsChange, 1) << " LU</td></tr></table></div></section>\n";
        std::printf ("report %s\n", name.toRawUTF8());
    }
    // Blind pairs: each Part 2 preset against the nearest Part 1 preset.
    const int pairs[][2] = { { 18, 12 }, { 19, 5 }, { 20, 13 }, { 21, 1 }, { 22, 6 }, { 23, 11 }, { 24, 10 }, { 25, 12 }, { 26, 13 }, { 28, 16 }, { 29, 15 } };
    juce::String pairJs = "[";
    for (const auto& p : pairs)
        pairJs << "[" << p[0] << "," << p[1] << "],";
    pairJs << "]";
    juce::String page;
    page << R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>stereophield listening report</title><style>
:root{--bg:#14161a;--panel:#1d2026;--text:#e6e8eb;--dim:#8b919a;--accent:#4fd1c5;--warn:#f56565}
body{background:var(--bg);color:var(--text);font:14px/1.45 -apple-system,system-ui,sans-serif;margin:0;padding:24px;max-width:1100px}
h1{font-size:22px}h2{font-size:16px;margin:0 0 6px}.preset,.blind{background:var(--panel);border-radius:8px;padding:14px 16px;margin:14px 0}
.desc{color:var(--dim);margin:0 0 10px}.cz{color:var(--dim);font-style:italic}.row{display:flex;flex-wrap:wrap;gap:14px;align-items:center}
.players label{display:block;color:var(--dim);font-size:12px}audio{display:block;height:32px;margin:2px 0 6px}img{border-radius:4px}
table td{padding:2px 10px 2px 0}table td:last-child{color:var(--accent)}button{background:#262a31;color:var(--text);border:1px solid #2c3038;border-radius:4px;padding:5px 12px;cursor:pointer}
button.on{background:var(--accent);color:var(--bg)}textarea{width:100%;height:120px;background:var(--bg);color:var(--text);border:1px solid #2c3038}
</style></head><body><h1>stereophield listening report</h1>
<p>Every factory preset on 6 s of the <code>mix</code> test signal (mono input, 48 kHz). Play the dry input, the processed stereo and the mono fold; the numbers are the objective measures of PART2_LEDGER.md I12. Use headphones and loudspeakers.</p>
<section class="blind"><h2>Blind A/B</h2><p class="desc">Each new preset against the closest preset of 1.0, under hidden labels. Listen to A and B, choose, note any artefacts, then reveal. Results stay in this browser and can be exported for docs/LISTENING.md.</p>
<div id="blind"></div><p><button onclick="exportResults()">Export results</button></p><textarea id="out" readonly></textarea></section>
)HTML" << rows << R"HTML(<script>
const pairs=)HTML" << pairJs << R"HTML(;const pad=n=>String(n).padStart(2,'0');
const store=JSON.parse(localStorage.getItem('sphBlind')||'{}');
function render(){const root=document.getElementById('blind');root.innerHTML='';pairs.forEach(([a,b],i)=>{
 if(!store[i])store[i]={swap:Math.random()<0.5};const s=store[i];const x=s.swap?b:a,y=s.swap?a:b;
 const d=document.createElement('div');d.className='row';d.style.margin='8px 0';
 d.innerHTML=`<strong>Pair ${i+1}</strong> <label>A <audio controls preload="none" src="${pad(x)}.wav"></audio></label>
 <label>B <audio controls preload="none" src="${pad(y)}.wav"></audio></label>
 <button class="${s.choice==='A'?'on':''}" data-c="A">Prefer A</button><button class="${s.choice==='B'?'on':''}" data-c="B">Prefer B</button>
 <button class="${s.choice==='='?'on':''}" data-c="=">Tie</button><input placeholder="artefacts heard" value="${s.note||''}">
 <span class="reveal">${s.revealed?`A = preset ${x}, B = preset ${y}`:'<button data-r="1">Reveal</button>'}</span>`;
 d.querySelectorAll('button[data-c]').forEach(btn=>btn.onclick=()=>{s.choice=btn.dataset.c;save();});
 const r=d.querySelector('button[data-r]');if(r)r.onclick=()=>{s.revealed=true;save();};
 d.querySelector('input').onchange=e=>{s.note=e.target.value;save();};root.appendChild(d);});}
function save(){localStorage.setItem('sphBlind',JSON.stringify(store));render();}
function exportResults(){const lines=pairs.map(([a,b],i)=>{const s=store[i]||{};const pick=!s.choice?'-':s.choice==='='?'tie':((s.choice==='A')!==!!s.swap?'new (preset '+a+')':'old (preset '+b+')');
 return `| ${a} vs ${b} | ${pick} | ${s.note||''} |`;});document.getElementById('out').value='| Pair | Preferred | Artefacts |\n| --- | --- | --- |\n'+lines.join('\n');}
render();</script></body></html>)HTML";
    dir.getChildFile ("index.html").replaceWithText (page);
    std::printf ("wrote %s\n", dir.getChildFile ("index.html").getFullPathName().toRawUTF8());
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    sph::ui::setPreferencesPersistent (false); // never touch the user's settings
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
        else if (a == "--report" && i + 1 < argc)
        {
            writeReport (juce::File::getCurrentWorkingDirectory().getChildFile (argv[++i]));
            return 0;
        }
        else if (a == "--metrics")
        {
            juce::File::getCurrentWorkingDirectory().getChildFile ("docs/PRESET_METRICS.md").replaceWithText (measure::presetMetricsMarkdown());
            return 0;
        }
        else if (a == "--make-fixtures" && i + 1 < argc)
        {
            makeFixtures (juce::File::getCurrentWorkingDirectory().getChildFile (argv[++i]));
            return 0;
        }
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
