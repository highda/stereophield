#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace sph::ui
{
// Language system (PART2_LEDGER.md L1). Every visible text comes from one
// table of English and Czech strings, keyed by stable ASCII identifiers.
enum class Language { English = 0, Czech = 1 };

Language language() noexcept;
void setLanguage (Language l) noexcept;

// Short text: labels, captions, choice items, buttons. Unknown keys return
// the key itself (P2-T1 checks that none are unknown).
juce::String tr (const char* key);
juce::String tr (const juce::String& key);
bool hasString (const juce::String& key);

// A number formatted for the current language: decimal comma in Czech.
juce::String localNumber (const juce::String& englishText);

// Long help for the info panel (L2). Fields are in the current language.
struct Help
{
    juce::String title, what, how, tryThis, ref;
};
bool help (const juce::String& key, Help& out);
bool hasHelp (const juce::String& key);

// For tests: every key of each table, and both languages of an entry.
std::vector<juce::String> stringKeys();
std::vector<juce::String> helpKeys();
juce::String stringIn (const juce::String& key, Language l);
bool helpIn (const juce::String& key, Language l, Help& out);

// Global preference (shared by instances), stored in
// ~/Library/Application Support/highda/stereophield.settings.
// Tests turn persistence off so they never change the user's settings file.
void setPreferencesPersistent (bool on) noexcept;
void loadLanguagePreference();
void saveLanguagePreference();
int savedLanguagePreference();
// Other global interface preferences in the same file.
double savedSetting (const char* name, double fallback);
void saveSetting (const char* name, double value);
} // namespace sph::ui
