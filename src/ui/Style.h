#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace sph::ui
{
// Visual style
namespace colours
{
inline const juce::Colour background { 0xff14161a };
inline const juce::Colour panel { 0xff1d2026 };
inline const juce::Colour text { 0xffe6e8eb };
inline const juce::Colour secondary { 0xff8b919a };
inline const juce::Colour accent { 0xff4fd1c5 };
inline const juce::Colour warning { 0xfff56565 };
inline const juce::Colour track { 0xff2c3038 };
inline const juce::Colour control { 0xff262a31 };
} // namespace colours

inline constexpr float bodySize = 13.0f;
inline constexpr float labelSize = 11.0f;
inline constexpr float cornerRadius = 8.0f;

inline juce::Font bodyFont() { return juce::Font (juce::FontOptions (bodySize)); }
inline juce::Font labelFont() { return juce::Font (juce::FontOptions (labelSize)); }
inline juce::Font titleFont() { return juce::Font (juce::FontOptions (bodySize, juce::Font::bold)); }

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return labelFont(); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return bodyFont(); }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return labelFont(); }
};
} // namespace sph::ui
