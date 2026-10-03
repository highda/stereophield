#pragma once

#include <cmath>

namespace sph
{
// y = a * y + (1 - a) * x with a = exp(-1 / (tau * rate)).
class OnePole
{
public:
    static double coefficient (double tauSeconds, double rate) noexcept
    {
        if (tauSeconds <= 0.0 || rate <= 0.0)
            return 0.0;
        return std::exp (-1.0 / (tauSeconds * rate));
    }

    void setTimeConstant (double tauSeconds, double rate) noexcept { a = coefficient (tauSeconds, rate); }
    void setCoefficient (double coeff) noexcept { a = coeff; }
    double getCoefficient() const noexcept { return a; }

    void reset (double value = 0.0) noexcept { y = value; }

    double process (double x) noexcept
    {
        y = a * y + (1.0 - a) * x;
        return y;
    }

    double current() const noexcept { return y; }

private:
    double a = 0.0;
    double y = 0.0;
};

// One-pole envelope follower with separate rising and falling coefficients.
class Follower
{
public:
    void setTimes (double riseSeconds, double fallSeconds, double rate) noexcept
    {
        rise = OnePole::coefficient (riseSeconds, rate);
        fall = OnePole::coefficient (fallSeconds, rate);
    }

    void reset (double value = 0.0) noexcept { y = value; }

    double process (double x) noexcept
    {
        const double a = x > y ? rise : fall;
        y = a * y + (1.0 - a) * x;
        return y;
    }

    double current() const noexcept { return y; }

private:
    double rise = 0.0, fall = 0.0, y = 0.0;
};
} // namespace sph
