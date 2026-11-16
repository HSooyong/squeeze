#include "StaticCurve.h"

#include "Tunables.h"

#include <algorithm>
#include <cmath>

namespace squeeze
{

float StaticCurve::softKneeGainDb(float detectorDb, float thresholdDb, float ratio, float kneeDb) noexcept
{
    const float safeRatio = std::max(ratio, 1.0f);
    const float slope = (1.0f / safeRatio) - 1.0f;
    const float over = detectorDb - thresholdDb;

    if (kneeDb <= 0.0f)
        return over > 0.0f ? slope * over : 0.0f;

    const float half = kneeDb * 0.5f;
    if (over <= -half)
        return 0.0f;
    if (over >= half)
        return slope * over;

    const float position = over + half;
    return slope * (position * position) / (2.0f * kneeDb);
}

float StaticCurve::optoGainDb(float detectorDb, float limitMix) noexcept
{
    const float mix = std::clamp(limitMix, 0.0f, 1.0f);
    const float compressed = softKneeGainDb(detectorDb,
                                             tunables::optoThresholdDb,
                                             tunables::optoCompressRatio,
                                             tunables::optoCompressKneeDb);
    const float limited = softKneeGainDb(detectorDb,
                                          tunables::optoThresholdDb,
                                          tunables::optoLimitRatio,
                                          tunables::optoLimitKneeDb);
    return compressed + (limited - compressed) * mix;
}

float StaticCurve::vcaGainDb(float detectorDb, float thresholdDb, float ratio) noexcept
{
    return softKneeGainDb(detectorDb, thresholdDb, ratio, 0.0f);
}

float StaticCurve::fetGainDb(float detectorDb, float ratio, float kneeDb) noexcept
{
    return softKneeGainDb(detectorDb, tunables::fetThresholdDb, ratio, kneeDb);
}

float StaticCurve::variMuRatio(float detectorDb, float thresholdDb) noexcept
{
    const float over = std::max(0.0f, detectorDb - thresholdDb);
    return std::min(tunables::vmuRatioMax, tunables::vmuRatioBase + tunables::vmuRatioSlope * over);
}

float StaticCurve::variMuGainDb(float detectorDb, float thresholdDb) noexcept
{
    return softKneeGainDb(detectorDb, thresholdDb, variMuRatio(detectorDb, thresholdDb), tunables::vmuKneeDb);
}

} // namespace squeeze
