#pragma once

namespace sph
{
enum class Engine { Light, Full };
enum class Source { Full, Tonal, Noise, TonalNoise };
enum class SpreadType { Delay, Cascade };
enum class HaasSide { Right, Left };
enum class ModType { Chorus, MicroPitch };
enum class PanMode { Static, Tracks, Groups };
enum class CompMode { MonoExact, ConstantLoudness };
enum class Listen { Stereo, Mono, Side };

// Every parameter of DESIGN.md section 8 in processing units: percentages are
// fractions (0 to 1, or 0 to 2 for widths), times in ms, frequencies in Hz.
struct Params
{
    Engine engine = Engine::Light;
    float width = 1.0f;
    float midBlend = 0.0f;
    float bassMonoHz = 150.0f;
    float bandXoverLo = 400.0f;
    float bandXoverHi = 4000.0f;
    float bandLow = 1.0f, bandMid = 1.0f, bandHigh = 1.0f;
    float transientDuck = 0.5f;
    bool guard = true;
    CompMode compMode = CompMode::MonoExact;
    float outGainDb = 0.0f;
    Listen listen = Listen::Stereo;
    bool bypass = false;
    float ambience = 0.5f;
    float roomDecayS = 1.0f;

    float spreadAmount = 0.6f;
    Source spreadSource = Source::Full;
    SpreadType spreadType = SpreadType::Cascade;
    float spreadTimeMs = 10.0f;
    int spreadDensity = 8;
    float spreadFLo = 100.0f, spreadFHi = 10000.0f;
    float spreadSkew = 0.0f, spreadQ = 0.7f;

    float delayAmount = 0.0f;
    Source delaySource = Source::Full;
    float delayTimeMs = 15.0f;
    float delayLpHz = 20000.0f;
    HaasSide delaySide = HaasSide::Right;

    float modAmount = 0.0f;
    Source modSource = Source::Full;
    ModType modType = ModType::Chorus;
    float modRateHz = 0.4f, modDepthMs = 1.5f, modBaseMs = 8.0f;
    float modCents = 9.0f, modPredelayMs = 12.0f;

    float velvetAmount = 0.0f;
    Source velvetSource = Source::Full;
    float velvetSizeMs = 30.0f;
    float velvetDensity = 1000.0f;
    int velvetVariation = 0;

    float panAmount = 0.0f;
    PanMode panMode = PanMode::Groups;
    float panDepth = 0.7f;
    float panDensity = 1.0f;
    float panBassCenterHz = 120.0f;
    int panMaxGroups = 6;
};

// Generators, in the order of the signal-flow diagram.
enum GeneratorId { genSpread = 0, genDelay, genMod, genVelvet, genPan, numGenerators };
} // namespace sph
