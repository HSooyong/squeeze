#pragma once

namespace squeeze
{

// Reported latency does not depend on the selected mode.
// M1 has no oversampler, so the delay is zero. A later milestone may change this
// when the oversampling factor changes, and must keep the value identical across modes.
inline constexpr int kReportedLatencySamples = 0;

} // namespace squeeze
