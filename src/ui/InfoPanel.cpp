#include "ui/InfoPanel.h"

#include "plugin/Parameters.h"

namespace sph::ui
{
const std::vector<Tour>& tours()
{
    static const std::vector<Tour> t {
        { "Mid and side", "Střed a strana", {
            { "Stereo can be described as left and right, or as mid (L + R, what both share) and side (L - R, what differs). This plugin never touches the mid: it only adds side.",
              "Stereo lze popsat jako levý a pravý kanál, nebo jako střed (L + R, co mají společné) a stranu (L - R, čím se liší). Tento plugin střed nikdy nemění: jen přidává stranu.", { "disp.goniometer" }, {} },
            { "On the goniometer, mid points straight up and side points sideways. A mono signal is a vertical line.",
              "Na goniometru střed míří nahoru a strana do stran. Mono signál je svislá čára.", { "disp.goniometer" }, { { ids::spread_amount, 0 } } },
            { "Raise the Spread amount: the line opens into a shape leaning left and right. That opening is the side signal.",
              "Zvyšte množství rozprostření: čára se otevře do tvaru nakloněného vlevo a vpravo. To otevření je boční signál.", { "p.spread_amount", "disp.goniometer" }, { { ids::spread_amount, 80 } } },
            { "Width scales all the side at once. The correlation meter shows how similar the channels stay: +1 is mono, 0 is fully different.",
              "Šíře škáluje celou stranu najednou. Korelační měřič ukazuje, jak podobné kanály zůstávají: +1 je mono, 0 zcela odlišné.", { "p.width", "disp.correlation" }, {} } } },
        { "Mono compatibility", "Mono kompatibilita", {
            { "Many listeners hear mono: phones, kitchen radios, club systems. Mono is (L + R) / 2, which cancels the side completely.",
              "Mnoho posluchačů slyší mono: telefony, kuchyňská rádia, klubové ozvučení. Mono je (L + R) / 2, což stranu úplně vyruší.", { "p.listen" }, {} },
            { "Set Listen to Mono and turn Width up and down: nothing changes, because the mid is untouched. That is what mono-safe means.",
              "Přepněte Poslech na Mono a měňte Šíři: nic se nezmění, protože střed je nedotčen. To znamená mono-kompatibilní.", { "p.listen", "p.width" }, { { ids::listen, 1 } } },
            { "Mid blend adds each effect's mid part back. It sounds richer in stereo, but in mono you now hear combs and chorusing. The Bands view shows the mono-fold deviation.",
              "Příměs středu vrací do středu složku každého efektu. Ve stereu zní bohatěji, ale v mono teď slyšíte hřebeny a chorus. Zobrazení Pásma ukazuje odchylku mono složení.", { "p.mid_blend", "tab.bands" }, { { ids::mid_blend, 100 }, { ids::listen, 1 } } },
            { "Return Mid blend to 0 % and Listen to Stereo.", "Vraťte Příměs středu na 0 % a Poslech na Stereo.", { "p.mid_blend" }, { { ids::mid_blend, 0 }, { ids::listen, 0 } } } } },
        { "Haas and comb filters", "Haas a hřebenové filtry", {
            { "A copy delayed by a few milliseconds is fused with the original and heard from the earlier side: the precedence effect.",
              "Kopie zpožděná o pár milisekund splyne s originálem a je slyšet ze strany, která přišla dřív: efekt přednosti.", { "card.delay" }, { { ids::spread_amount, 0 }, { ids::delay_amount, 100 }, { ids::mid_blend, 100 } } },
            { "Adding a signal to its delayed copy makes a comb filter: notches every 1 / delay Hz. Hover the Delay time to see the spacing.",
              "Součet signálu s jeho zpožděnou kopií vytvoří hřebenový filtr: zářezy každých 1 / zpoždění Hz. Najeďte na čas zpoždění a uvidíte rozestup.", { "p.delay_time_ms" }, {} },
            { "Listen in Mono now: the comb is plainly audible. This is why classic Haas widening fails in mono.",
              "Poslouchejte teď v mono: hřeben je zřetelně slyšet. Proto klasické Haasovo rozšíření v mono selhává.", { "p.listen" }, { { ids::listen, 1 } } },
            { "Set Mid blend to 0 %: the comb leaves the mono sum and lives only in the side. Half the depth, but mono-safe.",
              "Nastavte Příměs středu na 0 %: hřeben opustí mono součet a zůstane jen ve straně. Poloviční hloubka, ale mono-kompatibilní.", { "p.mid_blend" }, { { ids::mid_blend, 0 } } } } },
        { "Decorrelation", "Dekorelace", {
            { "Two signals with the same spectrum but different fine structure sound wide and diffuse. Measuring how similar they are gives the coherence.",
              "Dva signály se stejným spektrem, ale jinou jemnou strukturou zní široce a difúzně. Míra jejich podobnosti je koherence.", { "disp.correlation" }, {} },
            { "Velvet convolves each side with a different sparse click sequence. Watch the correlation fall.",
              "Samet konvoluuje každou stranu jinou řídkou sekvencí impulsů. Sledujte, jak korelace klesá.", { "card.velvet", "disp.correlation" }, { { ids::spread_amount, 0 }, { ids::velvet_amount, 100 } } },
            { "The coherence designer lets you choose the coherence per frequency, for example that of two microphones 40 cm apart (Full engine).",
              "Návrhář koherence vám dovolí zvolit koherenci po kmitočtech, například jako u dvou mikrofonů 40 cm od sebe (plné jádro).", { "card.coherence", "tab.coherence" }, { { ids::engine, 1 }, { ids::velvet_amount, 0 }, { ids::coh_amount, 100 }, { ids::coh_mode, 1 } } },
            { "The perceived-width meter shows the catch: on speakers, crosstalk between the ears limits how wide anything can sound.",
              "Měřič vnímané šíře ukazuje háček: na reproduktorech přeslech mezi ušima omezuje, jak široce cokoli může znít.", { "disp.asw" }, {} } } },
        { "Spectral panning", "Spektrální panoráma", {
            { "Switch the teaching source to Two sources from the settings menu, then apply this step.",
              "Přepněte v nastavení výukový zdroj na Dva zdroje a pak použijte tento krok.", { "card.pan" }, { { ids::engine, 1 }, { ids::spread_amount, 0 }, { ids::pan_amount, 100 }, { ids::pan_mode, 0 }, { ids::pan_depth, 100 } } },
            { "Static mode splits the spectrum into zones panned alternately left and right. See them on the Pan map tab.",
              "Statický režim rozdělí spektrum na pásma střídavě vlevo a vpravo. Uvidíte je na kartě Pan. mapa.", { "tab.pan", "p.pan_density" }, {} },
            { "Groups mode finds the harmonics of each source and gives each source one position: two instruments, two places.",
              "Režim Skupiny najde harmonické každého zdroje a dá každému zdroji jednu pozici: dva nástroje, dvě místa.", { "p.pan_mode", "tab.pan" }, { { ids::pan_mode, 2 } } } } },
        { "Smart disable", "Chytré vypínání", {
            { "Modules that cannot affect the output are not computed: zero amount, or a silent input for longer than their tail.",
              "Moduly, které nemohou ovlivnit výstup, se nepočítají: nulové množství nebo ticho na vstupu delší než jejich doznívání.", { "card.spread" }, {} },
            { "Open Scopes and choose Activity in a lane: each row is a module, filled while it runs.",
              "Otevřete Průběhy a v jedné stopě zvolte Aktivitu: každý řádek je modul, vyplněný, dokud běží.", { "tab.scopes" }, {} },
            { "Stop playback or mute the input: modules fall asleep one by one as their tails end. The dot on each card goes dark.",
              "Zastavte přehrávání nebo ztlumte vstup: moduly usínají jeden po druhém, jak končí jejich doznívání. Tečka na každé kartě zhasne.", { "card.spread", "card.velvet" }, {} } } },
    };
    return t;
}

InfoPanel::InfoPanel()
{
    for (auto* b : { &pin, &back, &next, &apply, &close })
        addAndMakeVisible (*b);
    pin.setClickingTogglesState (true);
    back.onClick = [this] { if (tour >= 0 && step > 0) { --step; if (onTourChanged) onTourChanged(); repaint(); resized(); } };
    next.onClick = [this]
    {
        if (tour >= 0 && step + 1 < (int) tours()[(size_t) tour].steps.size())
            ++step;
        if (onTourChanged)
            onTourChanged();
        repaint();
        resized();
    };
    apply.onClick = [this] { if (auto* s = currentStep(); s != nullptr && onApply) onApply (*s); };
    close.onClick = [this]
    {
        tour = -1;
        listing = false;
        if (onTourChanged)
            onTourChanged();
        resized();
        repaint();
    };
    for (size_t i = 0; i < tours().size(); ++i)
    {
        auto* b = tourButtons.add (new juce::TextButton());
        b->onClick = [this, i] { startTour ((int) i); };
        addChildComponent (b);
    }
    setHelpKey (*this, "ui.learn");
    localise();
}

void InfoPanel::localise()
{
    pin.setButtonText (tr ("ui.pin"));
    back.setButtonText (tr ("ui.back"));
    next.setButtonText (tr ("ui.next"));
    apply.setButtonText (tr ("ui.apply"));
    close.setButtonText (tr ("ui.close"));
    for (int i = 0; i < tourButtons.size(); ++i)
    {
        const auto& t = tours()[(size_t) i];
        tourButtons[i]->setButtonText (juce::String::fromUTF8 (language() == Language::Czech ? t.titleCs : t.titleEn));
    }
    repaint();
}

void InfoPanel::show (const juce::String& helpKey, const juce::String& liveText)
{
    if (isPinned() || tour >= 0 || listing)
        return;
    if (helpKey == key && liveText == live)
        return;
    key = helpKey;
    live = liveText;
    repaint();
}

void InfoPanel::openTourList()
{
    listing = true;
    tour = -1;
    resized();
    repaint();
}

void InfoPanel::startTour (int index)
{
    listing = false;
    tour = index;
    step = 0;
    if (onTourChanged)
        onTourChanged();
    resized();
    repaint();
}

const TourStep* InfoPanel::currentStep() const
{
    return tour >= 0 ? &tours()[(size_t) tour].steps[(size_t) step] : nullptr;
}

void InfoPanel::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto bottom = r.removeFromBottom (24);
    const bool inT = tour >= 0;
    pin.setVisible (! inT && ! listing);
    for (auto* b : { &back, &next, &apply })
        b->setVisible (inT);
    close.setVisible (inT || listing);
    if (inT)
    {
        const int w = bottom.getWidth() / 4;
        back.setBounds (bottom.removeFromLeft (w).reduced (2, 0));
        next.setBounds (bottom.removeFromLeft (w).reduced (2, 0));
        apply.setBounds (bottom.removeFromLeft (w).reduced (2, 0));
        close.setBounds (bottom.reduced (2, 0));
        apply.setEnabled (currentStep() != nullptr && ! currentStep()->apply.empty());
    }
    else if (listing)
        close.setBounds (bottom.removeFromRight (80));
    else
        pin.setBounds (bottom.removeFromRight (80));
    auto list = r.withTrimmedTop (40);
    for (auto* b : tourButtons)
    {
        b->setVisible (listing);
        b->setBounds (list.removeFromTop (28).reduced (0, 2));
    }
}

juce::AttributedString InfoPanel::compose() const
{
    juce::AttributedString a;
    a.setWordWrap (juce::AttributedString::byWord);
    auto add = [&a] (const juce::String& t, const juce::Font& f, juce::Colour c) { a.append (t, f, c); };
    const auto heading = juce::Font (juce::FontOptions (11.0f, juce::Font::bold));
    if (tour >= 0)
    {
        const auto& t = tours()[(size_t) tour];
        const auto& s = t.steps[(size_t) step];
        add (juce::String::fromUTF8 (language() == Language::Czech ? t.titleCs : t.titleEn) + "\n", juce::Font (juce::FontOptions (15.0f, juce::Font::bold)), colours::text);
        add (tr ("ui.step") + " " + juce::String (step + 1) + " / " + juce::String ((int) t.steps.size()) + "\n\n", heading, colours::secondary);
        add (juce::String::fromUTF8 (language() == Language::Czech ? s.cs : s.en), bodyFont(), colours::text);
        return a;
    }
    if (listing)
    {
        add (tr ("ui.tours"), juce::Font (juce::FontOptions (15.0f, juce::Font::bold)), colours::text);
        return a;
    }
    Help h;
    if (! help (key, h))
        return a;
    add (h.title + "\n\n", juce::Font (juce::FontOptions (15.0f, juce::Font::bold)), colours::text);
    if (h.what.isNotEmpty())
    {
        add (tr ("ui.whatitdoes") + "\n", heading, colours::secondary);
        add (h.what + "\n\n", bodyFont(), colours::text);
    }
    if (live.isNotEmpty())
    {
        add (tr ("ui.now") + "\n", heading, colours::secondary);
        add (live + "\n\n", bodyFont(), colours::accent);
    }
    if (h.how.isNotEmpty())
    {
        add (tr ("ui.how") + "\n", heading, colours::secondary);
        add (h.how + "\n\n", bodyFont(), colours::text);
    }
    if (h.tryThis.isNotEmpty())
    {
        add (tr ("ui.try") + "\n", heading, colours::secondary);
        add (h.tryThis + "\n\n", bodyFont(), colours::text);
    }
    if (h.ref.isNotEmpty())
    {
        add (tr ("ui.ref") + "\n", heading, colours::secondary);
        add (h.ref, labelFont(), colours::secondary);
    }
    return a;
}

void InfoPanel::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), cornerRadius);
    auto r = getLocalBounds().reduced (12).withTrimmedBottom (30).toFloat();
    juce::TextLayout layout;
    layout.createLayout (compose(), r.getWidth());
    layout.draw (g, r.withHeight (std::min (r.getHeight(), layout.getHeight())));
}

float InfoPanel::textHeight (const juce::String& helpKey, float width)
{
    InfoPanel p;
    p.key = helpKey;
    p.live = "At 15.0 ms the comb notches are 66.7 Hz apart; the first is at 33.3 Hz.";
    juce::TextLayout layout;
    layout.createLayout (p.compose(), width);
    return layout.getHeight();
}
} // namespace sph::ui
