#pragma once

#include "plugin/Parameters.h"

#include <array>

namespace sph::easy
{
// Easy mode mapping (PART3_LEDGER.md section 4): four macros drive the
// complete parameter set through monotone piecewise-linear curves, one set per
// material class, blended by the classifier's weights.

inline constexpr int numPoints = 5; // curve control points at 0, 25, 50, 75, 100 %
enum Class { percussive = 0, tonal, mixed, numClasses };

struct Curve
{
    float y[numPoints];

    // Linear interpolation over x in 0 .. 1.
    float at (float x) const noexcept;
};

struct ClassTable
{
    Curve width;       // `width` (percent) against the Width macro
    Curve charGain;    // factor on it against Character, evening out width
    float spread, cohPhase; // generator amounts (percent) of the three families:
    float velvet, coh;      //   phase (clean), decorrelation,
    float dbl, mod;         //   movement,
    float pan;              //   and the pan map (constant)
};

// [low latency][class]
struct Table
{
    ClassTable cls[2][numClasses];
};

// The optimised table (EasyTables.h, written by sph_easy_opt).
const Table& table();

struct Macros
{
    float width = 0.5f, character = 0.4f, space = 0.2f, focus = 0.5f; // 0 .. 1
    bool adapt = true, lowLatency = false;
};

using Weights = std::array<float, numClasses>;

bool isEasy (const float* values) noexcept;
Macros readMacros (const float* values) noexcept;

// Writes the value of every core parameter the mapping sets into `values`
// (plain values in ids::all order); the others are set to `defaults`, except
// bypass, which stays as it is. Weights are used only when the macros' Adapt
// is on; otherwise the mixed set applies. Allocation-free.
void apply (float* values, const float* defaults, const Weights& weights, bool stereoInput,
            const Table& t = table()) noexcept;
} // namespace sph::easy
