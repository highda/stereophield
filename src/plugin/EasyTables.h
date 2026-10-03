#pragma once

// Easy mode table, written by sph_easy_opt --optimise --write (PART3_LEDGER.md
// section 4.2). Do not edit by hand: change tools/easy_opt.cpp and rerun.
// Target: ASW = Width x min (0.400, the item's reachable width), loudspeaker listener.
// Full: J 350.22 -> 113.68 (width 106.87, rising 3.30; correlation 0.10, loudness 0.00, bass 0.00; smoothness 3.400; transients, not minimised, 978.81)
// Low latency: J 274.79 -> 75.85 (width 73.82, rising 0.66; correlation 0.00, loudness 0.00, bass 0.00; smoothness 1.380; transients, not minimised, 115.41)

#include "plugin/EasyMode.h"

namespace sph::easy
{
// Per class: width curve, Character gain curve, then the amounts of
// spread, coherence (clean), velvet, coherence (diffuse), double, mod, pan.
inline constexpr Table generatedTable { {
    { // Full engine
      { { { 0, 18.327, 57.38, 163.375, 200 } }, { { 2.06, 2.281, 1.879, 1.761, 0.817 } }, 40, 70, 40, 90, 50, 0, 0 }, // percussive
      { { { 0, 48.062, 79.349, 123.856, 200 } }, { { 0.53, 0.731, 0.416, 0.547, 0.33 } }, 80, 0, 60, 60, 90, 70, 60 }, // tonal
      { { { 0, 55.94, 76.784, 99.283, 136.859 } }, { { 0.337, 0.511, 0.3, 0.514, 0.467 } }, 100, 30, 70, 70, 80, 40, 30 } } // mixed
    ,
    { // Low latency (Light engine): no spectral generators
      { { { 0, 44.511, 89.284, 149.466, 200 } }, { { 0.948, 1.402, 1.283, 1.388, 1.034 } }, 100, 0, 80, 0, 50, 0, 0 }, // percussive
      { { { 0, 0, 0, 0, 6.745 } }, { { 2.518, 1.947, 1.371, 0.816, 0.3 } }, 80, 0, 90, 0, 90, 70, 0 }, // tonal
      { { { 0, 70.119, 102.831, 142.742, 187.872 } }, { { 0.496, 0.84, 0.726, 0.942, 0.625 } }, 100, 0, 100, 0, 80, 40, 0 } } // mixed
} };
} // namespace sph::easy
