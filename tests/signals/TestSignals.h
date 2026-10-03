#pragma once

// Generated test signals of DESIGN.md section 10.2. All deterministic.

#include "dsp/Rng.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace sph::signals
{
using Signal = std::vector<float>;

inline int samples (double seconds, double fs) { return (int) std::lround (seconds * fs); }

inline double rms (const float* x, size_t n)
{
    double s = 0.0;
    for (size_t i = 0; i < n; ++i)
        s += (double) x[i] * x[i];
    return n > 0 ? std::sqrt (s / (double) n) : 0.0;
}
inline double rms (const Signal& x) { return rms (x.data(), x.size()); }

inline double peak (const Signal& x)
{
    double p = 0.0;
    for (float v : x)
        p = std::max (p, (double) std::abs (v));
    return p;
}

inline void normalisePeak (Signal& x, double target)
{
    const double p = peak (x);
    if (p > 0.0)
        for (auto& v : x)
            v = (float) (v * target / p);
}

// One sample of amplitude 0.25, then zeros.
inline Signal impulse (int length)
{
    Signal x ((size_t) length, 0.0f);
    if (length > 0)
        x[0] = 0.25f;
    return x;
}

// White Gaussian noise, RMS -12 dBFS, seed 1.
inline Signal noise (int length, uint32_t seed = 1)
{
    Rng rng (seed);
    const double g = std::pow (10.0, -12.0 / 20.0);
    Signal x ((size_t) length);
    for (auto& v : x)
        v = (float) (g * rng.gaussian());
    return x;
}

// Sine at frequency f, amplitude 0.25.
inline Signal sine (double f, int length, double fs, double amplitude = 0.25)
{
    Signal x ((size_t) length);
    for (int i = 0; i < length; ++i)
        x[(size_t) i] = (float) (amplitude * std::sin (2.0 * std::numbers::pi * f * i / fs));
    return x;
}

// One sample of amplitude 0.5 every 250 ms, starting at n = 0.
inline Signal clicks (int length, double fs)
{
    Signal x ((size_t) length, 0.0f);
    const int period = samples (0.25, fs);
    for (int i = 0; i < length; i += period)
        x[(size_t) i] = 0.5f;
    return x;
}

// 440 Hz, amplitude 0.25, steady for 1 s, then decays 60 dB per second for 1 s.
inline Signal decayTone (double fs)
{
    const int steady = samples (1.0, fs), total = samples (2.0, fs);
    Signal x ((size_t) total);
    for (int i = 0; i < total; ++i)
    {
        const double t = (double) i / fs;
        const double env = i < steady ? 1.0 : std::pow (10.0, -3.0 * (t - 1.0));
        x[(size_t) i] = (float) (0.25 * env * std::sin (2.0 * std::numbers::pi * 440.0 * t));
    }
    return x;
}

// Harmonics 1 to 8 at amplitude 1/h of f0, added into x from start to end
// with raised-cosine fades of the given lengths.
inline void addHarmonicTone (Signal& x, double fs, double f0, int start, int end,
                             int fadeIn, int fadeOut)
{
    for (int i = std::max (0, start); i < std::min ((int) x.size(), end); ++i)
    {
        const double t = (double) (i - start) / fs;
        double env = 1.0;
        if (fadeIn > 0 && i - start < fadeIn)
            env = 0.5 - 0.5 * std::cos (std::numbers::pi * (i - start) / fadeIn);
        if (fadeOut > 0 && end - i <= fadeOut)
            env *= 0.5 - 0.5 * std::cos (std::numbers::pi * (end - i - 1) / fadeOut);
        double v = 0.0;
        for (int h = 1; h <= 8; ++h)
            v += std::sin (2.0 * std::numbers::pi * h * f0 * t) / h;
        x[(size_t) i] += (float) (env * v);
    }
}

inline constexpr double twoSourceF0A = 220.0;
inline constexpr double twoSourceF0B = 277.18;

// Source A at 220 Hz from 0.5 s, source B at 277.18 Hz from 1.0 s, 3 s long,
// 10 ms fade-ins, peak normalised to 0.5.
inline Signal twoSource (double fs)
{
    const int total = samples (3.0, fs), fade = samples (0.010, fs);
    Signal x ((size_t) total, 0.0f);
    addHarmonicTone (x, fs, twoSourceF0A, samples (0.5, fs), total, fade, 0);
    addHarmonicTone (x, fs, twoSourceF0B, samples (1.0, fs), total, fade, 0);
    normalisePeak (x, 0.5);
    return x;
}

inline constexpr double melodyNotes[4] = { 220.0, 247.0, 262.0, 294.0 };

// Note i occupies [melodyNoteStart(i), melodyNoteStart(i) + 300 ms).
inline int melodyNoteStart (int i, double fs) { return samples (0.320 * i, fs); }

// Four notes of 300 ms with 5 ms fades and 20 ms gaps, peak normalised to 0.5.
inline Signal melody (double fs)
{
    const int note = samples (0.300, fs), fade = samples (0.005, fs);
    Signal x ((size_t) samples (4 * 0.320, fs), 0.0f);
    for (int i = 0; i < 4; ++i)
    {
        const int s = melodyNoteStart (i, fs);
        addHarmonicTone (x, fs, melodyNotes[i], s, s + note, fade, fade);
    }
    normalisePeak (x, 0.5);
    return x;
}

// 440 Hz sine at -18 dBFS peak with one click of amplitude 0.9 at 1.0 s; 2 s long.
inline Signal toneClick (double fs)
{
    Signal x = sine (440.0, samples (2.0, fs), fs, std::pow (10.0, -18.0 / 20.0));
    x[(size_t) samples (1.0, fs)] += 0.9f;
    return x;
}

// noise for 2 s, silence for 3 s, noise for 2 s.
inline Signal gapNoise (double fs)
{
    const int a = samples (2.0, fs), gap = samples (3.0, fs);
    Signal n = noise (2 * a);
    Signal x ((size_t) (2 * a + gap), 0.0f);
    std::copy (n.begin(), n.begin() + a, x.begin());
    std::copy (n.begin() + a, n.end(), x.begin() + a + gap);
    return x;
}

// twoSource plus noise at -30 dB plus clicks at -12 dB; 3 s long.
inline Signal mix (double fs)
{
    Signal x = twoSource (fs);
    const Signal n = noise ((int) x.size());
    const Signal c = clicks ((int) x.size(), fs);
    const double gn = std::pow (10.0, -30.0 / 20.0), gc = std::pow (10.0, -12.0 / 20.0);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = (float) (x[i] + gn * n[i] + gc * c[i]);
    return x;
}

// x repeated to the given length.
inline Signal tile (const Signal& x, int length)
{
    Signal y ((size_t) length);
    for (int i = 0; i < length; ++i)
        y[(size_t) i] = x[(size_t) i % x.size()];
    return y;
}

inline double toDb (double ratio) { return 20.0 * std::log10 (std::max (ratio, 1e-30)); }
} // namespace sph::signals
