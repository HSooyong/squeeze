#pragma once

namespace squeeze::ui
{

// Design size shared with the rest of the Bearfield plugin family.
inline constexpr int kDefaultWidth = 1280;
inline constexpr int kMinWidth = 1100;
inline constexpr int kMaxWidth = 1760;
inline constexpr int kDesignHeight = 660;
inline constexpr int kHeaderHeight = 56;
inline constexpr int kLeftColumnWidth = 200;

// Idle and animating UI rates. The editor stays at the idle rate until a frame arrives.
inline constexpr int kIdleTimerHz = 4;
inline constexpr int kActiveTimerHz = 60;

// Positive half-up rounding of width * 660 / 1280.
inline constexpr int heightForWidth(int width) noexcept
{
    return static_cast<int>((static_cast<long long>(width) * kDesignHeight + (kDefaultWidth / 2)) / kDefaultWidth);
}

} // namespace squeeze::ui
