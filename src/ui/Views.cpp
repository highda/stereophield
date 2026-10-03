#include "ui/Views.h"

#include "dsp/CoherenceDesigner.h"
#include "plugin/Parameters.h"

#include <cmath>

namespace sph::ui
{
namespace
{
void frame (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (colours::background);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::track);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

const char* generatorTitle (int g)
{
    static const char* keys[] = { "card.spread", "card.delay", "card.mod", "card.velvet", "card.pan", "card.coherence", "card.double", "card.room" };
    return g >= 0 && g < 8 ? keys[g] : "";
}
} // namespace

void drawTap (juce::Graphics& g, juce::Rectangle<float> r, const ScopeTaps& taps, int tap, double seconds,
              int64_t endColumn, juce::Colour colour, bool unipolar)
{
    const int cols = (int) (seconds * ScopeTaps::columnsPerSecond);
    const int64_t written = taps.columnsWritten (tap);
    const int64_t last = std::min (endColumn, written);
    const int64_t first = std::max<int64_t> ({ 0, last - cols, written - ScopeTaps::ringColumns + 1 });
    // Peak scale over the visible span, with a floor so silence stays flat.
    float peak = unipolar ? 1.0f : 0.05f;
    if (! unipolar)
        for (int64_t c = first; c < last; ++c)
            peak = std::max ({ peak, std::abs (taps.columnMin (tap, c)), std::abs (taps.columnMax (tap, c)) });
    const float mid = unipolar ? r.getBottom() : r.getCentreY();
    const float half = unipolar ? r.getHeight() : r.getHeight() * 0.5f;
    g.setColour (colours::track);
    g.drawHorizontalLine ((int) mid, r.getX(), r.getRight());
    g.setColour (colour);
    const float w = r.getWidth() / (float) cols;
    for (int64_t c = first; c < last; ++c)
    {
        const float x = r.getRight() - (float) (last - c) * w;
        const float lo = taps.columnMin (tap, c) / peak, hi = taps.columnMax (tap, c) / peak;
        const float y0 = mid - juce::jlimit (-1.0f, 1.0f, hi) * half, y1 = mid - juce::jlimit (-1.0f, 1.0f, lo) * half;
        g.fillRect (x, std::min (y0, y1), std::max (1.0f, w), std::max (1.0f, std::abs (y1 - y0)));
    }
}

void MiniScope::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    frame (g, r);
    drawTap (g, r.reduced (2.0f), taps, tap, 2.0, taps.columnsWritten (ScopeTaps::outL),
             (dim ? colours::secondary : colours::accent).withAlpha (0.8f));
}

// ------------------------------------------------------------ OutputAnalysis
void OutputAnalysis::prepare (double sampleRate)
{
    fs = sampleRate;
    sb.prepare (fs, 0.3);
    vl.prepare (fs, 0.4, mode);
    for (auto* v : { &l, &r, &m })
        v->assign ((size_t) (2 * StereoBands::frameSize + vl.windowSize()), 0.0f);
    for (auto* v : { &tl, &tr, &tm })
        v->assign (8192, 0.0f);
    fill = sinceBands = sinceListener = 0;
}

void OutputAnalysis::setPlayback (VirtualListener::Playback p)
{
    mode = p;
    vl.prepare (fs, 0.4, mode);
}

void OutputAnalysis::pull (OutputRing& ring)
{
    const int cap = (int) l.size();
    for (;;)
    {
        const int n = ring.pop (tl.data(), tr.data(), tm.data(), (int) tl.size());
        if (n == 0)
            break;
        for (int i = 0; i < n; ++i)
        {
            // Rolling buffers: the newest sample at the end.
            if (fill == cap)
            {
                std::move (l.begin() + 1, l.end(), l.begin());
                std::move (r.begin() + 1, r.end(), r.begin());
                std::move (m.begin() + 1, m.end(), m.begin());
                --fill;
            }
            l[(size_t) fill] = tl[(size_t) i];
            r[(size_t) fill] = tr[(size_t) i];
            m[(size_t) fill] = tm[(size_t) i];
            ++fill;
            if (++sinceBands >= StereoBands::frameSize / 2 && fill >= StereoBands::frameSize)
            {
                sinceBands = 0;
                const int at = fill - StereoBands::frameSize;
                sb.addFrame (l.data() + at, r.data() + at, m.data() + at);
            }
            if (++sinceListener >= vl.windowSize() && fill >= vl.windowSize())
            {
                sinceListener = 0;
                const int at = fill - vl.windowSize();
                vl.addWindow (l.data() + at, r.data() + at);
            }
        }
    }
}

float OutputAnalysis::worstMonoFoldDb() const noexcept
{
    float worst = 0.0f;
    for (int b = 0; b < StereoBands::numBands; ++b)
        if (StereoBands::bandCentre (b) < 0.45 * fs && std::abs (sb.monoFoldDb (b)) > std::abs (worst))
            worst = sb.monoFoldDb (b);
    return worst;
}

// ---------------------------------------------------------------- ScopeLanes
ScopeLanes::ScopeLanes (const ScopeTaps& t) : taps (t)
{
    const int defaults[numLanes] = { ScopeTaps::inM, ScopeTaps::sideSyn, ScopeTaps::envDuck, ScopeTaps::envGuard, ScopeTaps::outL, activityTap };
    for (int i = 0; i < numLanes; ++i)
    {
        for (int tp = 0; tp <= ScopeTaps::numTaps; ++tp)
            pickers[(size_t) i].addItem (laneName (tp), tp + 1);
        pickers[(size_t) i].setSelectedId (defaults[i] + 1, juce::dontSendNotification);
        pickers[(size_t) i].onChange = [this] { repaint(); };
        addAndMakeVisible (pickers[(size_t) i]);
    }
    span.addItem ("2 s", 1);
    span.addItem ("5 s", 2);
    span.addItem ("10 s", 3);
    span.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (span);
    frozen.setClickingTogglesState (true);
    addAndMakeVisible (frozen);
    setHelpKey (*this, "disp.scopes");
    localise();
}

juce::String ScopeLanes::laneName (int tap) const
{
    if (tap == activityTap)
        return tr ("ui.activity");
    return ScopeTaps::tapId (tap);
}

void ScopeLanes::localise()
{
    for (auto& p : pickers)
    {
        // getSelectedId() reads 0 once the shown text no longer matches.
        const int id = p.getSelectedId();
        p.changeItemText (activityTap + 1, tr ("ui.activity"));
        if (id != 0)
            p.setSelectedId (id, juce::dontSendNotification);
    }
    frozen.setButtonText (tr ("ui.freeze"));
    for (int i = 0; i < numLanes; ++i)
        pickers[(size_t) i].setTitle (tr ("ui.lane") + " " + juce::String (i + 1));
    span.setTitle (tr ("ui.span"));
    repaint();
}

void ScopeLanes::showTap (int tap)
{
    pickers[0].setSelectedId (tap + 1);
    repaint();
}

void ScopeLanes::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (112);
    auto bottom = left.removeFromBottom (24);
    span.setBounds (bottom.removeFromLeft (54));
    frozen.setBounds (bottom.withTrimmedLeft (4));
    const int h = left.getHeight() / numLanes;
    for (int i = 0; i < numLanes; ++i)
        pickers[(size_t) i].setBounds (left.removeFromTop (h).reduced (0, 3));
}

void ScopeLanes::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().withTrimmedLeft (116).toFloat();
    const double seconds = span.getSelectedId() == 3 ? 10.0 : (span.getSelectedId() == 2 ? 5.0 : 2.0);
    const float h = r.getHeight() / numLanes;
    for (int i = 0; i < numLanes; ++i)
    {
        auto lane = r.removeFromTop (h).reduced (0, 2);
        frame (g, lane);
        const int tap = pickers[(size_t) i].getSelectedId() - 1;
        if (tap == activityTap)
        {
            // One row per module: filled while awake.
            const int rows = 11;
            const int cols = (int) (seconds * ScopeTaps::columnsPerSecond);
            const int64_t last = end, first = std::max<int64_t> ({ 0, last - cols, taps.columnsWritten (ScopeTaps::outL) - ScopeTaps::ringColumns + 1 });
            const float rh = (lane.getHeight() - 4) / rows, w = lane.getWidth() / (float) cols;
            const int bits[rows] = { 0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18 };
            for (int64_t c = first; c < last; ++c)
            {
                const auto mask = taps.activity (c);
                const float x = lane.getRight() - (float) (last - c) * w;
                for (int k = 0; k < rows; ++k)
                    if (mask & (1u << bits[k]))
                    {
                        g.setColour (colours::accent.withAlpha (0.35f + 0.05f * (float) (k % 3)));
                        g.fillRect (x, lane.getY() + 2 + k * rh, std::max (1.0f, w), rh - 1);
                    }
            }
            continue;
        }
        const bool unipolar = tap == ScopeTaps::envDuck || tap == ScopeTaps::envGuard;
        drawTap (g, lane.reduced (2.0f), taps, tap, seconds, end, colours::accent.withAlpha (0.85f), unipolar);
    }
}

// ------------------------------------------------------------------ FlowView
FlowView::FlowView (const ScopeTaps& t, std::function<bool (int)> awake) : taps (t), isAwake (std::move (awake))
{
    setHelpKey (*this, "disp.flow");
}

std::vector<FlowView::Node> FlowView::layoutNodes() const
{
    const auto r = getLocalBounds().toFloat().reduced (6.0f);
    const float colW = r.getWidth() / 6.0f;
    std::vector<Node> n;
    auto box = [&] (int col, float y, float h) { return juce::Rectangle<float> (r.getX() + col * colW + 6, r.getY() + y, colW - 12, h); };
    const float H = r.getHeight();
    n.push_back ({ "flow.input", "p.engine", ScopeTaps::inM, -1, box (0, H * 0.35f, H * 0.3f) });
    n.push_back ({ "flow.analysis", "p.engine", ScopeTaps::busTonal, -1, box (1, H * 0.35f, H * 0.3f) });
    const float gh = H / 8.0f;
    const int genTaps[8] = { ScopeTaps::genSpread, ScopeTaps::genDelay, ScopeTaps::genMod, ScopeTaps::genVelvet,
                             ScopeTaps::genPan, ScopeTaps::genCoherence, ScopeTaps::genDouble, ScopeTaps::genRoom };
    for (int g = 0; g < 8; ++g)
        n.push_back ({ generatorTitle (g), generatorTitle (g), genTaps[g], g, box (2, g * gh + 1, gh - 3) });
    n.push_back ({ "flow.sidebus", "p.bass_mono_hz", ScopeTaps::sideSyn, -1, box (3, H * 0.35f, H * 0.3f) });
    n.push_back ({ "flow.dry", "p.mid_blend", ScopeTaps::inM, -1, box (3, H * 0.75f, H * 0.2f) });
    n.push_back ({ "flow.output", "p.listen", ScopeTaps::outL, -1, box (4, H * 0.35f, H * 0.3f) });
    n.push_back ({ "flow.meters", "disp.goniometer", ScopeTaps::outS, -1, box (5, H * 0.35f, H * 0.3f) });
    return n;
}

void FlowView::paint (juce::Graphics& g)
{
    const auto nodes = layoutNodes();
    const int64_t end = taps.columnsWritten (ScopeTaps::outL);
    auto level = [&] (int tap)
    {
        // Peak of the last 0.25 s on a 60 dB scale, for the edge thickness.
        const int64_t w = taps.columnsWritten (tap);
        float p = 0.0f;
        for (int64_t c = std::max<int64_t> (0, w - 100); c < w; ++c)
            p = std::max ({ p, std::abs (taps.columnMin (tap, c)), std::abs (taps.columnMax (tap, c)) });
        return juce::jlimit (0.0f, 1.0f, 1.0f + 20.0f * std::log10 (p + 1e-9f) / 60.0f);
    };
    auto edge = [&] (const Node& a, const Node& b, int tap)
    {
        const auto p0 = a.box.getCentre().withX (a.box.getRight()), p1 = b.box.getCentre().withX (b.box.getX());
        juce::Path path;
        path.startNewSubPath (p0);
        path.cubicTo (p0.translated (20, 0), p1.translated (-20, 0), p1);
        g.setColour (colours::accent.withAlpha (0.25f + 0.5f * level (tap)));
        g.strokePath (path, juce::PathStrokeType (1.0f + 4.0f * level (tap)));
    };
    // Input -> analysis -> generators -> side bus -> output; input -> dry -> output.
    for (size_t k = 2; k < 10; ++k)
    {
        edge (nodes[1], nodes[k], ScopeTaps::inM);
        edge (nodes[k], nodes[10], nodes[k].tap);
    }
    edge (nodes[0], nodes[1], ScopeTaps::inM);
    edge (nodes[0], nodes[11], ScopeTaps::inM);
    edge (nodes[10], nodes[12], ScopeTaps::sideSyn);
    edge (nodes[11], nodes[12], ScopeTaps::inM);
    edge (nodes[12], nodes[13], ScopeTaps::outL);

    for (const auto& nd : nodes)
    {
        const bool asleep = nd.generator >= 0 && isAwake && ! isAwake (nd.generator);
        g.setColour (colours::panel);
        g.fillRoundedRectangle (nd.box, 5.0f);
        g.setColour (helpKeyOf (*this) == nd.helpKey && hoverHelp == nd.helpKey ? colours::accent : colours::track);
        g.drawRoundedRectangle (nd.box, 5.0f, 1.0f);
        auto inner = nd.box.reduced (4.0f);
        g.setFont (labelFont());
        g.setColour (asleep ? colours::secondary : colours::text);
        const auto title = tr (nd.titleKey) + (asleep ? "  (" + tr ("ui.asleep") + ")" : juce::String());
        // Short nodes put the waveform beside the title, tall ones below it.
        const bool beside = inner.getHeight() < 36.0f;
        auto label = beside ? inner.removeFromLeft (inner.getWidth() * 0.5f) : inner.removeFromTop (12);
        g.drawFittedText (title, label.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        if (inner.getHeight() > 4)
            drawTap (g, inner, taps, nd.tap, 2.0, end, (asleep ? colours::secondary : colours::accent).withAlpha (0.8f));
    }
}

void FlowView::mouseMove (const juce::MouseEvent& e)
{
    juce::String h = "disp.flow";
    for (const auto& n : layoutNodes())
        if (n.box.contains (e.position))
            h = n.helpKey;
    if (h != hoverHelp)
    {
        hoverHelp = h;
        setHelpKey (*this, h);
        repaint();
    }
}

void FlowView::mouseUp (const juce::MouseEvent& e)
{
    for (const auto& n : layoutNodes())
        if (n.box.contains (e.position) && onOpenTap)
            onOpenTap (n.tap);
}

// ---------------------------------------------------------------- BandsView
void BandsView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto top = r.removeFromTop (r.getHeight() * 0.5f).reduced (4, 2);
    auto bottom = r.reduced (4, 2);
    frame (g, top);
    frame (g, bottom);
    g.setFont (labelFont());
    g.setColour (colours::secondary);
    g.drawText (tr ("ui.bandcorr"), top.reduced (6, 2), juce::Justification::topLeft);
    g.drawText (tr ("ui.monofold"), bottom.reduced (6, 2), juce::Justification::topLeft);
    const auto& sb = analysis.bands();
    const float w = (top.getWidth() - 12) / StereoBands::numBands;
    for (int b = 0; b < StereoBands::numBands; ++b)
    {
        const float x = top.getX() + 6 + b * w;
        // Correlation: from the centre line, red below zero.
        const float c = juce::jlimit (-1.0f, 1.0f, sb.correlation (b));
        const float cy = top.getCentreY() + 6, ch = (top.getHeight() - 20) * 0.5f;
        g.setColour (c < 0 ? colours::warning : colours::accent);
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x + 1, std::min (cy, cy - c * ch), x + w - 1, std::max (cy, cy - c * ch)));
        // Mono fold: -12 .. +6 dB around 0.
        const float f = juce::jlimit (-12.0f, 6.0f, sb.monoFoldDb (b));
        const float zy = bottom.getY() + 14 + (bottom.getHeight() - 18) * (6.0f / 18.0f);
        const float fy = zy - f / 18.0f * (bottom.getHeight() - 18);
        g.setColour (std::abs (f) < 0.1f ? colours::accent : colours::warning);
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x + 1, std::min (zy, fy) - 0.5f, x + w - 1, std::max (zy, fy) + 0.5f));
        if (b % 3 == 0)
        {
            g.setColour (colours::secondary);
            const double fc = StereoBands::bandCentre (b);
            g.drawText (fc >= 1000 ? juce::String (fc / 1000.0, fc >= 10000 ? 0 : 1) + "k" : juce::String ((int) fc),
                        juce::Rectangle<float> (x - 6, bottom.getBottom() - 12, w + 12, 11), juce::Justification::centred);
        }
    }
    g.setColour (colours::track);
    g.drawHorizontalLine ((int) (top.getCentreY() + 6), top.getX(), top.getRight());
}

// ------------------------------------------------------------ CoherenceView
CoherenceView::CoherenceView (APVTS& s, const CoherenceDesigner& d, double sampleRate) : state (s), coh (d), rate (sampleRate)
{
    const char* idsList[5] = { ids::coh_p63, ids::coh_p250, ids::coh_p1k, ids::coh_p4k, ids::coh_p16k };
    for (int i = 0; i < 5; ++i)
        if (auto* p = state.getParameter (idsList[i]))
            points[(size_t) i] = std::make_unique<juce::ParameterAttachment> (*p, [this, i] (float v) { values[(size_t) i] = v; repaint(); });
    for (auto& p : points)
        if (p != nullptr)
            p->sendInitialUpdate();
    setHelpKey (*this, "disp.coherence");
}

Params CoherenceView::currentParams() const
{
    // The target curve from the parameters, so it shows while the designer sleeps.
    auto v = [this] (const char* id) { return state.getRawParameterValue (id)->load(); };
    Params p;
    p.cohMode = (CohMode) (int) v (ids::coh_mode);
    p.cohPattern = (MicPattern) (int) v (ids::coh_pattern);
    p.cohSpacingCm = v (ids::coh_spacing_cm);
    p.cohAngleDeg = v (ids::coh_angle_deg);
    const char* pts[5] = { ids::coh_p63, ids::coh_p250, ids::coh_p1k, ids::coh_p4k, ids::coh_p16k };
    for (int i = 0; i < 5; ++i)
        p.cohPoints[i] = v (pts[i]);
    return p;
}

juce::Rectangle<float> CoherenceView::plot() const { return getLocalBounds().toFloat().reduced (30, 12).withTrimmedBottom (8); }
float CoherenceView::xOf (double f) const
{
    const auto p = plot();
    return p.getX() + (float) (std::log (f / 40.0) / std::log (20000.0 / 40.0)) * p.getWidth();
}
float CoherenceView::yOf (double c) const
{
    const auto p = plot();
    return p.getY() + (float) ((1.0 - c) / 2.0) * p.getHeight(); // +1 top, -1 bottom
}

void CoherenceView::paint (juce::Graphics& g)
{
    const auto p = plot();
    frame (g, getLocalBounds().toFloat());
    g.setFont (labelFont());
    for (double c : { 1.0, 0.5, 0.0, -0.5, -1.0 })
    {
        g.setColour (colours::track);
        g.drawHorizontalLine ((int) yOf (c), p.getX(), p.getRight());
        g.setColour (colours::secondary);
        g.drawText (localNumber (juce::String (c, 1)), juce::Rectangle<float> (2, yOf (c) - 6, 26, 12), juce::Justification::centredRight);
    }
    for (double f : { 100.0, 1000.0, 10000.0 })
    {
        g.setColour (colours::track);
        g.drawVerticalLine ((int) xOf (f), p.getY(), p.getBottom());
        g.setColour (colours::secondary);
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + " kHz" : juce::String ((int) f) + " Hz",
                    juce::Rectangle<float> (xOf (f) + 2, p.getBottom(), 50, 12), juce::Justification::centredLeft);
    }
    const int n = CoherenceDesigner::numBands;
    juce::Path target, achieved;
    for (int b = 0; b < n; ++b)
    {
        const double f = CoherenceDesigner::bandCentre (b, rate);
        const float x = xOf (f);
        const float yt = yOf (CoherenceDesigner::targetFor (currentParams(), f)), ya = yOf (juce::jlimit (-1.0f, 1.0f, coh.achievedCoherence (b)));
        if (b == 0)
        {
            target.startNewSubPath (x, yt);
            achieved.startNewSubPath (x, ya);
        }
        else
        {
            target.lineTo (x, yt);
            achieved.lineTo (x, ya);
        }
    }
    // The achieved curve only while the designer runs (all zero otherwise).
    bool running = false;
    for (int b = 0; b < n; ++b)
        running = running || coh.achievedCoherence (b) != 0.0f;
    g.setColour (colours::text.withAlpha (0.7f));
    if (running)
        g.strokePath (achieved, juce::PathStrokeType (1.5f));
    g.setColour (colours::accent);
    g.strokePath (target, juce::PathStrokeType (2.0f));
    g.setColour (colours::secondary);
    g.setColour (colours::accent);
    g.drawText (tr ("ui.target"), juce::Rectangle<float> (p.getRight() - 160, p.getY(), 75, 12), juce::Justification::centredRight);
    g.setColour (colours::text.withAlpha (0.7f));
    g.drawText (tr ("ui.achieved"), juce::Rectangle<float> (p.getRight() - 80, p.getY(), 80, 12), juce::Justification::centredRight);

    // Curve mode: the five points can be dragged.
    if (auto* mode = state.getRawParameterValue (ids::coh_mode); mode != nullptr && (int) mode->load() == 0)
    {
        const double fs[5] = { 63, 250, 1000, 4000, 16000 };
        for (int i = 0; i < 5; ++i)
        {
            g.setColour (dragging == i ? colours::text : colours::accent);
            g.fillEllipse (xOf (fs[i]) - 5, yOf (values[(size_t) i]) - 5, 10, 10);
        }
    }
}

void CoherenceView::mouseDown (const juce::MouseEvent& e)
{
    if (auto* mode = state.getRawParameterValue (ids::coh_mode); mode == nullptr || (int) mode->load() != 0)
        return;
    const double fs[5] = { 63, 250, 1000, 4000, 16000 };
    for (int i = 0; i < 5; ++i)
        if (e.position.getDistanceFrom ({ xOf (fs[i]), yOf (values[(size_t) i]) }) < 12.0f)
        {
            dragging = i;
            points[(size_t) i]->beginGesture();
        }
}

void CoherenceView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    const auto p = plot();
    const float c = juce::jlimit (-0.5f, 1.0f, 1.0f - 2.0f * (e.position.y - p.getY()) / p.getHeight());
    points[(size_t) dragging]->setValueAsPartOfGesture (c);
}

void CoherenceView::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        points[(size_t) dragging]->endGesture();
    dragging = -1;
    repaint();
}

// ----------------------------------------------------------------- AswMeter
AswMeter::AswMeter (OutputAnalysis& a) : analysis (a), mode (nullptr, {}, { "ui.speakers", "ui.headphones" })
{
    mode.select (0);
    mode.onSelect = [this] (int i)
    {
        analysis.setPlayback (i == 0 ? VirtualListener::Playback::Speakers : VirtualListener::Playback::Headphones);
    };
    addAndMakeVisible (mode);
    setHelpKey (*this, "disp.asw");
}

void AswMeter::resized()
{
    mode.setBounds (getLocalBounds().removeFromBottom (18).removeFromRight (150));
}

void AswMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    r.removeFromBottom (20);
    g.setFont (labelFont());
    g.setColour (colours::secondary);
    g.drawText (tr ("ui.asw"), r.removeFromLeft (90), juce::Justification::centredLeft);
    const float v = juce::jlimit (0.0f, 1.0f, analysis.listener().asw());
    g.drawText (localNumber (juce::String (v, 2)), r.removeFromRight (30), juce::Justification::centredRight);
    frame (g, r.reduced (2, 3));
    g.setColour (colours::accent);
    g.fillRect (r.reduced (4, 5).withWidth ((r.getWidth() - 8) * v));
}
} // namespace sph::ui
