#pragma once

#include "dsp/PartialTracker.h"

#include <array>

namespace sph
{
// Stage D of DESIGN.md section 6.2.6: groups partials that share a harmonic
// series and an onset, and gives each group one pan position.
class SourceGrouper
{
public:
    static constexpr int maxGroups = 8;
    static constexpr int historySize = 16;

    // Tunable constants.
    static constexpr double harmonicTolerance = 0.03;
    static constexpr int onsetWindow = 4;
    static constexpr double latePartialFactor = 0.3;
    static constexpr double fadingFactor = 0.25;
    static constexpr double memorySeconds = 0.4;

    struct Group
    {
        bool active = false;
        double f0 = 0.0;
        int onsetFrame = 0;
        double amp = 0.0, peakAmp = 0.0;
        int diedFrame = 0;
        double pan = 0.0;
        int numTracks = 0;
    };

    struct Memory
    {
        bool valid = false;
        double f0 = 0.0, pan = 0.0;
        int diedFrame = 0;
    };

    void prepare (double sampleRate, int hop);
    void reset();

    // Updates groups from the tracks and assigns ungrouped tracks.
    void process (std::array<PartialTracker::Track, PartialTracker::maxTracks>& tracks, int frame,
                  int maxActiveGroups, double bassCentreHz) noexcept;

    const std::array<Group, maxGroups>& groups() const noexcept { return slots; }
    int numActive() const noexcept;

private:
    double assignPan (double f0, int frame, double bassCentreHz, int exclude = -1) const noexcept;

    int memoryFrames = 38;
    std::array<Group, maxGroups> slots;
    std::array<Memory, historySize> history;
    int historyPos = 0;
    std::array<int, PartialTracker::maxTracks> order {};
};
} // namespace sph
