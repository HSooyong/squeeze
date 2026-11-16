#pragma once

namespace squeeze
{

// Index order matches the mode parameter: LAMP, NEEDLE, SPARK, TUBE.
enum class Mode
{
    opto = 0,
    vca = 1,
    fet = 2,
    variMu = 3
};

inline constexpr int kModeCount = 4;

inline Mode modeFromIndex(int index) noexcept
{
    if (index < 0 || index >= kModeCount)
        return Mode::opto;

    return static_cast<Mode>(index);
}

} // namespace squeeze
