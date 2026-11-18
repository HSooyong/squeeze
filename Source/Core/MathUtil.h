#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace squeeze
{

inline constexpr float kDenormalFloor = 1.0e-20f;
inline constexpr float kDbFloor = -160.0f;
inline constexpr float kLinearFloor = 1.0e-8f;

inline float flushDenormal(float value) noexcept
{
    return std::fabs(value) < kDenormalFloor ? 0.0f : value;
}

inline bool isBadFloat(float value) noexcept
{
    return !std::isfinite(value) || std::fpclassify(value) == FP_SUBNORMAL;
}

inline float onePoleCoeff(double sampleRate, float seconds) noexcept
{
    if (!(sampleRate > 0.0) || !(seconds > 1.0e-8f))
        return 1.0f;

    return 1.0f - std::exp(-1.0f / (seconds * static_cast<float>(sampleRate)));
}

inline float linearToDb(float magnitude) noexcept
{
    return 20.0f * std::log10(std::max(magnitude, kLinearFloor));
}

inline float powerToDb(float power) noexcept
{
    return 10.0f * std::log10(std::max(power, kLinearFloor * kLinearFloor));
}

inline float dbToGain(float db) noexcept
{
    return std::pow(10.0f, db / 20.0f);
}

inline float clampFloat(float value, float minimum, float maximum) noexcept
{
    return std::max(minimum, std::min(maximum, value));
}

// Peak hold over a short window. A steady tone reports its peak, and a step
// still attacks on the first sample. Allocates only in prepare().
class PeakHold
{
public:
    void prepare(double sampleRate, float windowSeconds) noexcept
    {
        const int width = std::max(1, static_cast<int>(std::lround(sampleRate * static_cast<double>(windowSeconds))));
        buffer.assign(static_cast<size_t>(width), 0.0f);
        write = 0;
        filled = 0;
        held = 0.0f;
    }

    void reset() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        write = 0;
        filled = 0;
        held = 0.0f;
    }

    float process(float sample) noexcept
    {
        if (buffer.empty())
            return std::fabs(sample);

        const float magnitude = std::fabs(sample);
        if (filled < static_cast<int>(buffer.size()))
        {
            buffer[static_cast<size_t>(write)] = magnitude;
            write = (write + 1) % static_cast<int>(buffer.size());
            ++filled;
            held = std::max(held, magnitude);
            return held;
        }

        const float leaving = buffer[static_cast<size_t>(write)];
        buffer[static_cast<size_t>(write)] = magnitude;
        write = (write + 1) % static_cast<int>(buffer.size());

        if (magnitude >= held)
            held = magnitude;
        else if (leaving >= held)
            held = *std::max_element(buffer.begin(), buffer.end());

        return held;
    }

    bool hasBadState() const noexcept
    {
        if (isBadFloat(held))
            return true;

        for (const float value : buffer)
            if (isBadFloat(value))
                return true;

        return false;
    }

private:
    std::vector<float> buffer;
    int write = 0;
    int filled = 0;
    float held = 0.0f;
};

struct Ramp
{
    float value = 0.0f;
    float coefficient = 1.0f;

    void prepare(double sampleRate, float seconds) noexcept
    {
        coefficient = onePoleCoeff(sampleRate, seconds);
    }

    void snap(float target) noexcept
    {
        value = target;
    }

    float next(float target) noexcept
    {
        value += coefficient * (target - value);
        value = flushDenormal(value);
        return value;
    }
};

} // namespace squeeze
