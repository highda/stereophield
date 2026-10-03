#include "ui/Strings.h"

#include <atomic>
#include <unordered_map>

namespace sph::ui
{
namespace
{
std::atomic<int> currentLanguage { 0 };

struct Pair
{
    const char* key;
    const char* en;
    const char* cs;
};

// Short texts. Czech terminology follows PART2_LEDGER.md section 13.
const Pair shortTexts[] = {
    // Header
    { "ui.engine", "Engine", "Jádro" },
    { "ui.latency", "Latency", "Latence" },
    { "ui.bypass", "Bypass", "Obejít" },
    { "ui.undo", "Undo", "Zpět" },
    { "ui.redo", "Redo", "Znovu" },
    { "ui.copyab", "A > B", "A > B" },
    { "ui.learn", "Learn", "Učení" },
    { "ui.info", "i", "i" },
    { "ui.presets", "Presets", "Předvolby" },
    { "ui.factory", "Factory", "Tovární" },
    { "ui.user", "User", "Uživatelské" },
    { "ui.save", "Save preset...", "Uložit předvolbu..." },
    { "ui.delete", "Delete user preset", "Smazat uživatelskou předvolbu" },
    { "ui.zoom", "Zoom", "Zvětšení" },
    { "ui.tooltips", "Tooltips while the info panel is open", "Bublinová nápověda při otevřeném panelu" },
    { "ui.settings", "Settings", "Nastavení" },
    { "ui.presetname", "Preset name", "Název předvolby" },
    { "ui.ok", "OK", "OK" },
    { "ui.cancel", "Cancel", "Zrušit" },
    { "ui.pin", "Pin", "Připnout" },
    { "ui.whatitdoes", "What it does", "Co dělá" },
    { "ui.now", "Right now", "Právě teď" },
    { "ui.how", "How it works", "Jak to funguje" },
    { "ui.try", "Try this", "Vyzkoušejte" },
    { "ui.ref", "Background", "Zdroje" },
    { "ui.awake", "awake", "běží" },
    { "ui.asleep", "asleep", "spí" },
    { "ui.notmonosafe", "not mono-safe", "není mono-kompatibilní" },
    { "ui.fullonly", "Full engine only", "Jen v plném jádru" },
    { "ui.stereoonly", "Stereo input only", "Jen pro stereo vstup" },
    { "ui.correlation", "correlation", "korelace" },
    { "ui.asw", "perceived width", "vnímaná šíře" },
    { "ui.speakers", "Speakers", "Reproduktory" },
    { "ui.headphones", "Headphones", "Sluchátka" },
    { "ui.guardlabel", "correlation guard", "hlídač korelace" },
    { "ui.freeze", "Freeze", "Zmrazit" },
    { "ui.span", "Span", "Rozsah" },
    { "ui.target", "target", "cíl" },
    { "ui.achieved", "achieved", "dosaženo" },
    { "ui.monofold", "mono fold (dB)", "mono složení (dB)" },
    { "ui.bandcorr", "correlation per band", "korelace po pásmech" },
    { "ui.activity", "Activity", "Aktivita" },
    { "ui.next", "Next", "Další" },
    { "ui.back", "Back", "Zpět" },
    { "ui.apply", "Apply", "Použít" },
    { "ui.close", "Close", "Zavřít" },
    { "ui.tours", "Guided tours", "Prohlídky s průvodcem" },
    { "ui.step", "Step", "Krok" },
    { "ui.teach", "Teaching source", "Výukový zdroj" },

    // Panels and cards
    { "card.spread", "SPREAD", "ROZPROSTŘENÍ" },
    { "card.delay", "DELAY", "ZPOŽDĚNÍ" },
    { "card.mod", "MOD", "MODULACE" },
    { "card.velvet", "VELVET", "SAMET" },
    { "card.pan", "PAN MAP", "PAN. MAPA" },
    { "card.coherence", "COHERENCE", "KOHERENCE" },
    { "card.double", "DOUBLE", "ZDVOJENÍ" },
    { "card.room", "ROOM", "PROSTOR" },
    { "card.image", "IMAGE", "OBRAZ" },
    { "panel.analysis", "ANALYSIS", "ANALÝZA" },
    { "panel.side", "SIDE BUS", "BOČNÍ SBĚRNICE" },
    { "panel.output", "OUTPUT", "VÝSTUP" },
    { "panel.info", "INFO", "INFO" },
    { "tab.details", "Details", "Detail" },
    { "tab.pan", "Pan map", "Pan. mapa" },
    { "tab.coherence", "Coherence", "Koherence" },
    { "tab.flow", "Flow", "Tok signálu" },
    { "tab.scopes", "Scopes", "Průběhy" },
    { "tab.bands", "Bands", "Pásma" },

    // Parameter captions (knobs and combos)
    { "cap.engine", "engine", "jádro" },
    { "cap.width", "WIDTH", "ŠÍŘE" },
    { "cap.mid_blend", "mid blend", "příměs středu" },
    { "cap.bass_mono_hz", "bass mono", "mono basy" },
    { "cap.band_xover_lo", "xover lo", "dělení dolní" },
    { "cap.band_xover_hi", "xover hi", "dělení horní" },
    { "cap.band_low", "low", "basy" },
    { "cap.band_mid", "mid", "středy" },
    { "cap.band_high", "high", "výšky" },
    { "cap.transient_duck", "duck", "útlum tr." },
    { "cap.guard", "guard", "hlídač" },
    { "cap.comp_mode", "comp", "komp." },
    { "cap.out_gain_db", "gain", "zisk" },
    { "cap.listen", "listen", "poslech" },
    { "cap.bypass", "bypass", "obejít" },
    { "cap.ambience", "ambience", "ambience" },
    { "cap.room_decay_s", "room decay", "dozvuk" },
    { "cap.spread_amount", "amount", "množství" },
    { "cap.spread_source", "source", "zdroj" },
    { "cap.spread_type", "type", "typ" },
    { "cap.spread_time_ms", "time", "čas" },
    { "cap.spread_density", "density", "hustota" },
    { "cap.spread_f_lo", "low", "dolní" },
    { "cap.spread_f_hi", "high", "horní" },
    { "cap.spread_skew", "skew", "zkosení" },
    { "cap.spread_q", "Q", "Q" },
    { "cap.delay_amount", "amount", "množství" },
    { "cap.delay_source", "source", "zdroj" },
    { "cap.delay_time_ms", "time", "čas" },
    { "cap.delay_lp_hz", "low-pass", "dolní propust" },
    { "cap.delay_side", "side", "strana" },
    { "cap.mod_amount", "amount", "množství" },
    { "cap.mod_source", "source", "zdroj" },
    { "cap.mod_type", "type", "typ" },
    { "cap.mod_rate_hz", "rate", "rychlost" },
    { "cap.mod_depth_ms", "depth", "hloubka" },
    { "cap.mod_base_ms", "base", "základ" },
    { "cap.mod_cents", "cents", "centy" },
    { "cap.mod_predelay_ms", "pre-delay", "předzpoždění" },
    { "cap.velvet_amount", "amount", "množství" },
    { "cap.velvet_source", "source", "zdroj" },
    { "cap.velvet_size_ms", "size", "délka" },
    { "cap.velvet_density", "density", "hustota" },
    { "cap.velvet_variation", "variation", "varianta" },
    { "cap.pan_amount", "amount", "množství" },
    { "cap.pan_mode", "mode", "režim" },
    { "cap.pan_depth", "depth", "hloubka" },
    { "cap.pan_density", "density", "hustota" },
    { "cap.pan_bass_center_hz", "bass ctr", "střed basů" },
    { "cap.pan_max_groups", "groups", "skupiny" },
    { "cap.coh_amount", "amount", "množství" },
    { "cap.coh_source", "source", "zdroj" },
    { "cap.coh_mode", "mode", "režim" },
    { "cap.coh_p63", "63 Hz", "63 Hz" },
    { "cap.coh_p250", "250 Hz", "250 Hz" },
    { "cap.coh_p1k", "1 kHz", "1 kHz" },
    { "cap.coh_p4k", "4 kHz", "4 kHz" },
    { "cap.coh_p16k", "16 kHz", "16 kHz" },
    { "cap.coh_spacing_cm", "spacing", "rozestup" },
    { "cap.coh_angle_deg", "angle", "úhel" },
    { "cap.coh_pattern", "pattern", "charakt." },
    { "cap.coh_transient", "transients", "transienty" },
    { "cap.dbl_amount", "amount", "množství" },
    { "cap.dbl_source", "source", "zdroj" },
    { "cap.dbl_offset_ms", "offset", "posun" },
    { "cap.dbl_drift_ms", "drift", "kolísání" },
    { "cap.dbl_drift_rate", "drift rate", "rychlost kol." },
    { "cap.dbl_pitch_cents", "pitch", "ladění" },
    { "cap.dbl_level_db", "level", "úroveň" },
    { "cap.dbl_tone_db", "tone", "barva" },
    { "cap.dbl_seed", "seed", "semínko" },
    { "cap.room_amount", "amount", "množství" },
    { "cap.room_source", "source", "zdroj" },
    { "cap.room_size", "size", "velikost" },
    { "cap.room_distance", "distance", "vzdálenost" },
    { "cap.room_absorb", "absorption", "pohltivost" },
    { "cap.room_order", "order", "řád" },
    { "cap.room_damp_hz", "damping", "tlumení" },
    { "cap.img_amount", "expansion", "rozšíření" },
    { "cap.img_diffuse", "diffuse", "difúzní" },
    { "cap.img_center_hz", "keep below", "ponechat pod" },
    { "cap.velvet_design", "design", "návrh" },
    { "cap.latency_mode", "latency", "latence" },
    { "cap.transient_mode", "detector", "detektor" },
    { "cap.pan_ownership", "bins", "biny" },

    // Choice items
    { "choice.engine.0", "Light", "Lehké" },
    { "choice.engine.1", "Full", "Plné" },
    { "choice.guard.0", "Off", "Vyp." },
    { "choice.guard.1", "On", "Zap." },
    { "choice.comp_mode.0", "Mono-exact", "Přesné mono" },
    { "choice.comp_mode.1", "Constant loudness", "Stálá hlasitost" },
    { "choice.listen.0", "Stereo", "Stereo" },
    { "choice.listen.1", "Mono", "Mono" },
    { "choice.listen.2", "Side", "Strana" },
    { "choice.source.0", "Full", "Celý" },
    { "choice.source.1", "Tonal", "Tónová" },
    { "choice.source.2", "Noise", "Šumová" },
    { "choice.source.3", "Tonal+Noise", "Tón.+šum." },
    { "choice.spread_type.0", "Delay", "Zpoždění" },
    { "choice.spread_type.1", "Cascade", "Kaskáda" },
    { "choice.delay_side.0", "Right", "Vpravo" },
    { "choice.delay_side.1", "Left", "Vlevo" },
    { "choice.mod_type.0", "Chorus", "Chorus" },
    { "choice.mod_type.1", "Micro-pitch", "Mikrotransp." },
    { "choice.pan_mode.0", "Static", "Statická" },
    { "choice.pan_mode.1", "Tracks", "Stopy" },
    { "choice.pan_mode.2", "Groups", "Skupiny" },
    { "choice.coh_mode.0", "Curve", "Křivka" },
    { "choice.coh_mode.1", "Spaced pair", "Rozložený pár" },
    { "choice.coh_mode.2", "Coincident pair", "Koincidenční pár" },
    { "choice.coh_mode.3", "Near-coincident", "Blízce koinc." },
    { "choice.coh_pattern.0", "Omni", "Kulová" },
    { "choice.coh_pattern.1", "Subcardioid", "Široká ledvina" },
    { "choice.coh_pattern.2", "Cardioid", "Ledvinová" },
    { "choice.coh_pattern.3", "Supercardioid", "Superledvina" },
    { "choice.coh_pattern.4", "Figure-8", "Osmičková" },
    { "choice.room_order.0", "1", "1" },
    { "choice.room_order.1", "2", "2" },
    { "choice.velvet_design.0", "Random", "Náhodný" },
    { "choice.velvet_design.1", "Optimised", "Optimalizovaný" },
    { "choice.latency_mode.0", "Per engine", "Podle jádra" },
    { "choice.latency_mode.1", "Always Full", "Vždy plná" },
    { "choice.transient_mode.0", "Envelope", "Obálka" },
    { "choice.transient_mode.1", "Spectral flux", "Spektrální tok" },
    { "choice.pan_ownership.0", "Hard", "Pevné" },
    { "choice.pan_ownership.1", "Soft", "Měkké" },
    { "choice.teach.0", "Off", "Vyp." },
    { "choice.teach.1", "Noise", "Šum" },
    { "choice.teach.2", "Two sources", "Dva zdroje" },
    { "choice.teach.3", "Melody", "Melodie" },
    { "choice.teach.4", "Tone and click", "Tón a lusk" },
    { "choice.teach.5", "Drum loop", "Bicí smyčka" },
};

std::unordered_map<std::string, const Pair*>& shortIndex()
{
    static std::unordered_map<std::string, const Pair*> index = []
    {
        std::unordered_map<std::string, const Pair*> m;
        for (const auto& p : shortTexts)
            m[p.key] = &p;
        return m;
    }();
    return index;
}
} // namespace

Language language() noexcept { return (Language) currentLanguage.load(); }
void setLanguage (Language l) noexcept { currentLanguage.store ((int) l); }

juce::String stringIn (const juce::String& key, Language l)
{
    const auto& idx = shortIndex();
    const auto it = idx.find (key.toStdString());
    if (it == idx.end())
        return key;
    return juce::String::fromUTF8 (l == Language::Czech ? it->second->cs : it->second->en);
}

juce::String tr (const juce::String& key) { return stringIn (key, language()); }
juce::String tr (const char* key) { return tr (juce::String (key)); }
bool hasString (const juce::String& key) { return shortIndex().count (key.toStdString()) > 0; }

std::vector<juce::String> stringKeys()
{
    std::vector<juce::String> keys;
    for (const auto& p : shortTexts)
        keys.emplace_back (p.key);
    return keys;
}

juce::String localNumber (const juce::String& text)
{
    if (language() != Language::Czech)
        return text;
    // Decimal comma: replace a point between digits.
    juce::String out;
    for (int i = 0; i < text.length(); ++i)
    {
        const auto c = text[i];
        const bool between = c == '.' && i > 0 && i + 1 < text.length() && juce::CharacterFunctions::isDigit (text[i - 1])
                             && juce::CharacterFunctions::isDigit (text[i + 1]);
        out << (between ? juce::String (",") : juce::String::charToString (c));
    }
    return out;
}

static juce::File settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support/highda/stereophield.settings");
}

void loadLanguagePreference()
{
    const auto f = settingsFile();
    if (auto xml = juce::XmlDocument::parse (f))
        setLanguage (xml->getIntAttribute ("language", 0) == 1 ? Language::Czech : Language::English);
}

void saveLanguagePreference()
{
    const auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    juce::XmlElement xml ("STEREOPHIELD_SETTINGS");
    if (auto old = juce::XmlDocument::parse (f))
        xml = *old;
    xml.setAttribute ("language", (int) language());
    xml.writeTo (f);
}
} // namespace sph::ui
