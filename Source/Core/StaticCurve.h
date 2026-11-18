#pragma once

namespace squeeze
{

// Gain applied to the signal, in dB. Zero is unity, negative values are gain reduction.
// The engine and any later curve display call these functions rather than a second copy.
class StaticCurve
{
public:
    static float softKneeGainDb(float detectorDb, float thresholdDb, float ratio, float kneeDb) noexcept;

    static float optoGainDb(float detectorDb, float limitMix) noexcept;
    static float vcaGainDb(float detectorDb, float thresholdDb, float ratio) noexcept;
    static float fetGainDb(float detectorDb, float ratio, float kneeDb) noexcept;
    static float variMuGainDb(float detectorDb, float thresholdDb) noexcept;
    static float variMuRatio(float detectorDb, float thresholdDb) noexcept;
};

} // namespace squeeze
