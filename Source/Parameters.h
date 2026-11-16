#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace squeeze::params
{

// Host automation ids. Do not rename them.
inline constexpr const char* mode = "mode";
inline constexpr const char* osFactor = "os_factor";
inline constexpr const char* abLevelMatch = "ab_level_match";
inline constexpr const char* optoPeakRed = "opto_peak_red";
inline constexpr const char* optoGain = "opto_gain";
inline constexpr const char* optoLimit = "opto_limit";
inline constexpr const char* vcaThreshold = "vca_threshold";
inline constexpr const char* vcaRatio = "vca_ratio";
inline constexpr const char* vcaOutput = "vca_output";
inline constexpr const char* fetInput = "fet_input";
inline constexpr const char* fetOutput = "fet_output";
inline constexpr const char* fetAttack = "fet_attack";
inline constexpr const char* fetRelease = "fet_release";
inline constexpr const char* fetRatio = "fet_ratio";
inline constexpr const char* fetAllEnabled = "fet_all_enabled";
inline constexpr const char* vmuInput = "vmu_input";
inline constexpr const char* vmuThreshold = "vmu_threshold";
inline constexpr const char* vmuTime = "vmu_time";
inline constexpr const char* vmuOutput = "vmu_output";

// The continuous ratio parameter uses this value as its top, displayed as infinity.
inline constexpr float kRatioInfinity = 1000.0f;

inline constexpr int kParameterVersionHint = 1;

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace squeeze::params
