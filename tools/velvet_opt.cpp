// Offline optimiser for velvet-noise decorrelator sequences (PART2_LEDGER.md I1).
//
// For each of the 16 variations and each impulse-count class, starts from the
// random sequences of DESIGN.md section 6.3.4 (same seeds) and improves the
// impulse positions (within their grid cells) and signs by coordinate
// descent. The gains keep the specified exponential decay. Cost:
//
//   J = sum over third-octave bands 125 Hz .. 16 kHz of the squared level
//       deviation (dB) of each side from 0 dB
//     + 400 * (zero-lag correlation)^2
//     + 25 * sum over bands of the squared band coherence of L and R
//
// at the reference rate 48 kHz with a grid spacing of 48 samples (density
// 1000 per second). Writes src/dsp/VelvetTables.h.
//
//   sph_velvet_opt <output header>

#include "dsp/Rng.h"

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdio>
#include <numbers>
#include <thread>
#include <vector>

namespace
{
constexpr double fs = 48000.0;
constexpr int gridSpacing = 48;
constexpr int nfft = 8192;
constexpr int counts[] = { 8, 12, 16, 20, 24, 30, 40, 50, 60, 80, 100, 120, 160, 200, 240 };
constexpr int numClasses = (int) std::size (counts);

struct Side
{
    std::vector<int> pos;
    std::vector<int> sign;
};

struct Bands
{
    std::vector<std::pair<int, int>> ranges; // bin ranges of third-octave bands
    Bands()
    {
        for (int b = -9; b <= 12; ++b)
        {
            const double fc = 1000.0 * std::pow (2.0, b / 3.0);
            const int k0 = (int) std::ceil (fc * std::pow (2.0, -1.0 / 6.0) * nfft / fs);
            const int k1 = (int) std::floor (fc * std::pow (2.0, 1.0 / 6.0) * nfft / fs);
            ranges.push_back ({ k0, std::max (k0, k1) });
        }
    }
};

struct Problem
{
    int count = 30, len = 1440;
    std::vector<double> gain;
    std::vector<std::complex<double>> twiddle; // e^{-j 2 pi k / nfft} powers via table
    const Bands* bands = nullptr;
    int kMax = 0;

    void init (int m, const Bands& b)
    {
        count = m;
        len = m * gridSpacing;
        bands = &b;
        gain.resize ((size_t) m);
        double e = 0;
        for (int j = 0; j < m; ++j)
        {
            gain[(size_t) j] = std::exp (-6.908 * j / m);
            e += gain[(size_t) j] * gain[(size_t) j];
        }
        for (auto& g : gain)
            g /= std::sqrt (e);
        kMax = b.ranges.back().second + 1;
        twiddle.resize ((size_t) nfft);
        for (int i = 0; i < nfft; ++i)
            twiddle[(size_t) i] = std::polar (1.0, -2.0 * std::numbers::pi * i / nfft);
    }

    std::complex<double> tw (long long k, long long p) const { return twiddle[(size_t) ((k * p) % nfft)]; }

    void spectrum (const Side& s, std::vector<std::complex<double>>& X) const
    {
        X.assign ((size_t) kMax, {});
        for (int j = 0; j < count; ++j)
            for (int k = 0; k < kMax; ++k)
                X[(size_t) k] += gain[(size_t) j] * (double) s.sign[(size_t) j] * tw (k, s.pos[(size_t) j]);
    }

    double cost (const std::vector<std::complex<double>>& L, const std::vector<std::complex<double>>& R,
                 const Side& sl, const Side& sr) const
    {
        double j = 0.0, worst = 0.0;
        for (const auto& [k0, k1] : bands->ranges)
        {
            double el = 0, er = 0;
            std::complex<double> x {};
            for (int k = k0; k <= k1; ++k)
            {
                el += std::norm (L[(size_t) k]);
                er += std::norm (R[(size_t) k]);
                x += L[(size_t) k] * std::conj (R[(size_t) k]);
            }
            const double n = (double) (k1 - k0 + 1);
            const double dl = 10.0 * std::log10 (el / n + 1e-12), dr = 10.0 * std::log10 (er / n + 1e-12);
            j += dl * dl + dr * dr + 25.0 * std::norm (x) / (el * er + 1e-24);
            worst = std::max ({ worst, std::abs (dl), std::abs (dr) });
        }
        // The criterion is a maximum, so the worst band is weighted too.
        j += 30.0 * worst * worst;
        double c0 = 0.0;
        for (int a = 0; a < count; ++a)
            for (int b = 0; b < count; ++b)
                if (sl.pos[(size_t) a] == sr.pos[(size_t) b])
                    c0 += gain[(size_t) a] * gain[(size_t) b] * sl.sign[(size_t) a] * sr.sign[(size_t) b];
        return j + 400.0 * c0 * c0;
    }
};

Side randomSide (int m, uint32_t seed)
{
    // Same construction as Velvet::build at the reference rate.
    sph::Rng rng (seed);
    Side s;
    const double td = gridSpacing;
    for (int j = 0; j < m; ++j)
    {
        const double r1 = rng.uniform(), r2 = rng.uniform();
        s.pos.push_back (std::clamp ((int) std::lround (j * td + r1 * (td - 1.0)), 0, m * gridSpacing - 1));
        s.sign.push_back (r2 < 0.5 ? -1 : 1);
    }
    return s;
}

// Coordinate descent over positions (within each grid cell) and signs.
void optimise (const Problem& pr, Side& sl, Side& sr)
{
    std::vector<std::complex<double>> L, R;
    pr.spectrum (sl, L);
    pr.spectrum (sr, R);
    double best = pr.cost (L, R, sl, sr);
    for (int sweep = 0; sweep < 10; ++sweep)
    {
        bool improved = false;
        for (int side = 0; side < 2; ++side)
        {
            Side& s = side == 0 ? sl : sr;
            auto& X = side == 0 ? L : R;
            for (int j = 0; j < pr.count; ++j)
            {
                const int cell0 = j * gridSpacing;
                const int oldPos = s.pos[(size_t) j], oldSign = s.sign[(size_t) j];
                int bestPos = oldPos, bestSign = oldSign;
                for (int cand = 0; cand < 2 * gridSpacing; ++cand)
                {
                    const int p = cell0 + cand / 2;
                    const int sg = cand % 2 == 0 ? oldSign : -oldSign;
                    if (p == oldPos && sg == oldSign)
                        continue;
                    // Incremental spectrum update.
                    const double g = pr.gain[(size_t) j];
                    for (int k = 0; k < pr.kMax; ++k)
                        X[(size_t) k] += g * ((double) sg * pr.tw (k, p) - (double) oldSign * pr.tw (k, oldPos));
                    s.pos[(size_t) j] = p;
                    s.sign[(size_t) j] = sg;
                    const double c = pr.cost (L, R, sl, sr);
                    if (c < best - 1e-9)
                    {
                        best = c;
                        bestPos = p;
                        bestSign = sg;
                    }
                    for (int k = 0; k < pr.kMax; ++k)
                        X[(size_t) k] -= g * ((double) sg * pr.tw (k, p) - (double) oldSign * pr.tw (k, oldPos));
                    s.pos[(size_t) j] = oldPos;
                    s.sign[(size_t) j] = oldSign;
                }
                if (bestPos != oldPos || bestSign != oldSign)
                {
                    const double g = pr.gain[(size_t) j];
                    for (int k = 0; k < pr.kMax; ++k)
                        X[(size_t) k] += g * ((double) bestSign * pr.tw (k, bestPos) - (double) oldSign * pr.tw (k, oldPos));
                    s.pos[(size_t) j] = bestPos;
                    s.sign[(size_t) j] = bestSign;
                    improved = true;
                }
            }
        }
        if (! improved)
            break;
    }
}

// Packs a position (as a fraction of the sequence length, 15 bits) and a sign.
uint16_t pack (int pos, int len, int sign)
{
    const int frac = std::clamp ((int) std::lround ((double) pos / len * 32768.0), 0, 32767);
    return (uint16_t) ((frac << 1) | (sign < 0 ? 1 : 0));
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::puts ("usage: sph_velvet_opt <output header>");
        return 1;
    }
    const Bands bands;
    struct Job { int var, cls; };
    std::vector<Job> jobs;
    for (int v = 0; v < 16; ++v)
        for (int c = 0; c < numClasses; ++c)
            jobs.push_back ({ v, c });
    std::vector<std::array<Side, 2>> results (jobs.size());
    std::atomic<size_t> next { 0 };
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < std::max (1u, std::thread::hardware_concurrency()); ++t)
        pool.emplace_back ([&]
        {
            for (size_t i; (i = next.fetch_add (1)) < jobs.size();)
            {
                const auto [v, c] = jobs[i];
                Problem pr;
                pr.init (counts[c], bands);
                // Four starts: the specified seeds, then three more; keep the best.
                double best = 1e300;
                for (uint32_t start = 0; start < 4; ++start)
                {
                    Side sl = randomSide (counts[c], 1000u + 2u * (uint32_t) v + 100000u * start);
                    Side sr = randomSide (counts[c], 1001u + 2u * (uint32_t) v + 100000u * start);
                    optimise (pr, sl, sr);
                    std::vector<std::complex<double>> L, R;
                    pr.spectrum (sl, L);
                    pr.spectrum (sr, R);
                    const double cst = pr.cost (L, R, sl, sr);
                    if (cst < best)
                    {
                        best = cst;
                        results[i] = { sl, sr };
                    }
                }
            }
        });
    for (auto& t : pool)
        t.join();

    juce::String h;
    h << "#pragma once\n\n// Generated by tools/velvet_opt.cpp (PART2_LEDGER.md I1). Do not edit.\n"
      << "// Optimised velvet sequences: for each variation (16) and impulse-count class,\n"
      << "// left then right, each impulse packed as (position fraction of the length << 1) | (sign < 0).\n\n"
      << "#include <cstdint>\n\nnamespace sph::velvet_tables\n{\n";
    h << "inline constexpr int numClasses = " << numClasses << ";\ninline constexpr int classCounts[numClasses] = { ";
    for (int c = 0; c < numClasses; ++c)
        h << counts[c] << (c + 1 < numClasses ? ", " : " };\n");
    int total = 0;
    for (int c : counts)
        total += c;
    h << "// Offset of class c within a variation's side: sum of the earlier counts.\n";
    h << "inline constexpr int sideLength = " << total << ";\n";
    h << "inline constexpr uint16_t data[16][2][sideLength] = {\n";
    for (int v = 0; v < 16; ++v)
    {
        h << "  {\n";
        for (int side = 0; side < 2; ++side)
        {
            h << "    {";
            int col = 0;
            for (int c = 0; c < numClasses; ++c)
            {
                const auto& s = results[(size_t) (v * numClasses + c)][(size_t) side];
                for (int j = 0; j < counts[c]; ++j)
                {
                    h << (col % 16 == 0 ? "\n     " : " ") << (int) pack (s.pos[(size_t) j], counts[c] * gridSpacing, s.sign[(size_t) j]) << ",";
                    ++col;
                }
            }
            h << "\n    },\n";
        }
        h << "  },\n";
    }
    h << "};\n} // namespace sph::velvet_tables\n";
    juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]).replaceWithText (h);
    std::printf ("wrote %s\n", argv[1]);
    return 0;
}
