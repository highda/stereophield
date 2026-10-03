#include "plugin/EasyMode.h"

#include "plugin/EasyTables.h"

#include <algorithm>
#include <cmath>

namespace sph::easy
{
float Curve::at (float x) const noexcept
{
    const float t = std::clamp (x, 0.0f, 1.0f) * (numPoints - 1);
    const int i = std::min ((int) t, numPoints - 2);
    const float f = t - (float) i;
    return y[i] + f * (y[i + 1] - y[i]);
}

const Table& table()
{
    return generatedTable;
}

namespace
{
#define SPH_I(name) [[maybe_unused]] constexpr int i_##name = indexOf (ids::name);
SPH_PARAM_IDS (SPH_I)
#undef SPH_I
} // namespace

bool isEasy (const float* v) noexcept
{
    return std::lround (v[i_ui_mode]) == 0;
}

Macros readMacros (const float* v) noexcept
{
    Macros m;
    m.width = v[i_easy_width] * 0.01f;
    m.character = v[i_easy_character] * 0.01f;
    m.space = v[i_easy_space] * 0.01f;
    m.focus = v[i_easy_focus] * 0.01f;
    m.adapt = std::lround (v[i_easy_adapt]) != 0;
    m.lowLatency = std::lround (v[i_easy_low_latency]) != 0;
    return m;
}

void apply (float* v, const float* defaults, const Weights& weightsIn, bool stereoInput, const Table& t) noexcept
{
    const Macros m = readMacros (v);
    const float bypass = v[i_bypass];
    for (int i = 0; i < numCoreParameters; ++i)
        v[i] = defaults[i];
    v[i_bypass] = bypass;

    Weights w = m.adapt ? weightsIn : Weights { 0.0f, 0.0f, 1.0f };
    const float sum = w[0] + w[1] + w[2];
    for (auto& x : w)
        x = sum > 0.0f ? x / sum : 1.0f / (float) numClasses;

    // Character: phase at 0 %, decorrelation at 50 %, movement at 100 %.
    const float c = m.character;
    const float phase = std::max (0.0f, 1.0f - 2.0f * c);
    const float decor = 1.0f - std::abs (2.0f * c - 1.0f);
    const float motion = std::max (0.0f, 2.0f * c - 1.0f);

    float width = 0.0f, spread = 0.0f, velvet = 0.0f, coh = 0.0f, dbl = 0.0f, mod = 0.0f, pan = 0.0f;
    for (int k = 0; k < numClasses; ++k)
    {
        const auto& ct = t.cls[m.lowLatency ? 1 : 0][k];
        width += w[(size_t) k] * ct.width.at (m.width) * ct.charGain.at (c);
        spread += w[(size_t) k] * ct.spread * phase;
        velvet += w[(size_t) k] * ct.velvet * decor;
        coh += w[(size_t) k] * (ct.coh * decor + ct.cohPhase * phase);
        dbl += w[(size_t) k] * ct.dbl * motion;
        mod += w[(size_t) k] * ct.mod * motion;
        pan += w[(size_t) k] * ct.pan;
    }

    // Fixed: mono-safe, guarded, the engine by latency.
    v[i_engine] = m.lowLatency ? 0.0f : 1.0f;
    v[i_latency_mode] = m.lowLatency ? 0.0f : 1.0f;
    v[i_mid_blend] = 0.0f;
    v[i_guard] = 1.0f;
    v[i_guard_ceiling_db] = -6.0f; // side power at most -6 dB: about +1 dB of level
    v[i_comp_mode] = 0.0f;
    v[i_listen] = 0.0f;
    v[i_width_mode] = 0.0f;
    v[i_velvet_design] = 1.0f;
    v[i_transient_mode] = 1.0f;
    v[i_spread_type] = 1.0f;
    v[i_mod_type] = 1.0f;
    v[i_pan_mode] = 2.0f;
    v[i_coh_mode] = 1.0f;

    // Width.
    v[i_width] = std::clamp (width, 0.0f, 200.0f);
    v[i_band_high] = 120.0f;
    v[i_spread_amount] = std::clamp (spread, 0.0f, 100.0f);
    v[i_velvet_amount] = std::clamp (velvet, 0.0f, 100.0f);
    v[i_coh_amount] = std::clamp (coh, 0.0f, 100.0f);
    v[i_dbl_amount] = std::clamp (dbl, 0.0f, 100.0f);
    v[i_mod_amount] = std::clamp (mod, 0.0f, 100.0f);
    v[i_pan_amount] = std::clamp (pan, 0.0f, 100.0f);
    v[i_img_amount] = stereoInput && ! m.lowLatency ? 100.0f + 100.0f * m.width : 100.0f;

    // Space: early reflections and a wider virtual pair.
    const float s = m.space;
    v[i_room_amount] = 80.0f * s;
    v[i_room_size] = 4.0f + 16.0f * s;
    v[i_room_distance] = 1.5f + 2.5f * s;
    v[i_coh_spacing_cm] = 20.0f * std::pow (5.0f, s);

    // Focus: protect the centre.
    const float f = m.focus;
    // Focus 0 still keeps the lows and the hits in check (loudness, bass,
    // correlation); 90 % ducking at Focus 50 % keeps hits 6 dB narrower than
    // the body.
    v[i_bass_mono_hz] = 100.0f * std::pow (2.5f, f);
    v[i_transient_duck] = f <= 0.5f ? 60.0f + 60.0f * f : 90.0f + 20.0f * (f - 0.5f);
    v[i_band_low] = 105.0f * (1.0f - 0.6f * f);
    v[i_coh_transient] = 30.0f + 65.0f * f;
}
} // namespace sph::easy
