#pragma once

// Easy mode corpus (PART3_LEDGER.md section 4.2): the generated signals and
// two longer synthesised programmes. Mono, deterministic, no recordings.

#include "dsp/Rng.h"
#include "dsp/TestSignals.h"

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace sph::corpus
{
using signals::Signal;

struct Item
{
    std::string name;
    Signal x;
};

namespace detail
{
// A band-limited sawtooth-like tone: harmonics up to 6 kHz at 1/h.
inline void addSaw (Signal& x, double fs, double f0, int start, int length, double gain, double attack, double release)
{
    const int hMax = std::max (1, (int) (6000.0 / f0));
    for (int i = 0; i < length && start + i < (int) x.size(); ++i)
    {
        const double t = i / fs, tEnd = (length - i) / fs;
        const double env = std::min ({ 1.0, t / attack, tEnd / release });
        double v = 0.0;
        for (int h = 1; h <= hMax; ++h)
            v += std::sin (2.0 * std::numbers::pi * h * f0 * t) / h;
        x[(size_t) (start + i)] += (float) (gain * env * v);
    }
}

inline double midiHz (int note) { return 440.0 * std::pow (2.0, (note - 69) / 12.0); }
} // namespace detail

// Pad chords (slow attack, slightly detuned pairs) under a melody; 8 s.
inline Signal padMelody (double fs)
{
    using namespace detail;
    Signal x ((size_t) signals::samples (8.0, fs), 0.0f);
    const int chords[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 55, 59, 62 }, { 52, 55, 59 } };
    const int bar = signals::samples (2.0, fs);
    for (int c = 0; c < 4; ++c)
        for (int note : chords[c])
            for (double detune : { -0.004, 0.004 })
                addSaw (x, fs, midiHz (note) * (1.0 + detune), c * bar, bar, 0.05, 0.4, 0.3);
    const int melodyNotes[16] = { 76, 74, 72, 74, 72, 71, 69, 71, 72, 74, 76, 79, 76, 74, 72, 71 };
    const int step = signals::samples (0.5, fs);
    for (int i = 0; i < 16; ++i)
        addSaw (x, fs, midiHz (melodyNotes[i]), i * step, step - signals::samples (0.03, fs), 0.12, 0.01, 0.05);
    signals::normalisePeak (x, 0.7);
    return x;
}

// Drums, bass, chords and a lead; 8 s.
inline Signal arrangement (double fs)
{
    using namespace detail;
    const int total = signals::samples (8.0, fs);
    Signal x = signals::tile (signals::drumLoop (fs), total);
    for (auto& v : x)
        v *= 0.6f;
    const int beat = signals::samples (0.6, fs);
    const int bassNotes[8] = { 33, 33, 36, 38, 33, 33, 31, 31 };
    for (int i = 0; i * beat < total; ++i)
        addSaw (x, fs, midiHz (bassNotes[i % 8]), i * beat, beat - signals::samples (0.05, fs), 0.12, 0.005, 0.05);
    const int chords[2][3] = { { 57, 60, 64 }, { 55, 59, 62 } };
    for (int c = 0; c * 4 * beat < total; ++c)
        for (int note : chords[c % 2])
            addSaw (x, fs, midiHz (note), c * 4 * beat, 4 * beat, 0.035, 0.05, 0.2);
    const int lead[8] = { 76, 79, 81, 79, 76, 74, 72, 74 };
    for (int i = 0; 2 * i * beat < total; ++i)
        addSaw (x, fs, midiHz (lead[i % 8]), 2 * i * beat, 2 * beat - signals::samples (0.05, fs), 0.07, 0.02, 0.1);
    signals::normalisePeak (x, 0.8);
    return x;
}

// The corpus, each item `seconds` long (tiled where shorter).
inline std::vector<Item> items (double fs, double seconds = 6.0)
{
    using namespace signals;
    const int n = samples (seconds, fs);
    return {
        { "noise", noise (n) },
        { "two sources", tile (twoSource (fs), n) },
        { "melody", tile (melody (fs), n) },
        { "tone and click", tile (toneClick (fs), n) },
        { "drum loop", tile (drumLoop (fs), n) },
        { "mix", tile (mix (fs), n) },
        { "pad and melody", tile (padMelody (fs), n) },
        { "arrangement", tile (arrangement (fs), n) },
    };
}
} // namespace sph::corpus
