#include "VariMuModel.h"

#include "StaticCurve.h"
#include "Tunables.h"

#include <algorithm>
#include <cmath>

namespace squeeze
{

void VariMuModel::prepare(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    peak.prepare(sampleRate, tunables::vmuPeakHoldSeconds);
    memoryCoeff = onePoleCoeff(sampleRate, tunables::vmuMemorySeconds);
    reset();
}

void VariMuModel::reset() noexcept
{
    peak.reset();
    gr = 0.0f;
    memory = 0.0f;
    detectorDb = kDbFloor;
    ratio = 1.0f;
    color = 0.0f;
}

void VariMuModel::seedGr(float grDb) noexcept
{
    gr = grDb;
}

float VariMuModel::process(float left, float right, int numChannels, const VariMuSettings& settings) noexcept
{
    const float magnitude = numChannels > 1 ? std::max(std::fabs(left), std::fabs(right)) : std::fabs(left);
    const float held = peak.process(magnitude);
    detectorDb = linearToDb(held) + settings.inputDb;
    ratio = StaticCurve::variMuRatio(detectorDb, settings.thresholdDb);

    const float target = -StaticCurve::variMuGainDb(detectorDb, settings.thresholdDb);
    float releaseSeconds = settings.releaseSeconds;
    if (settings.slowReleaseSeconds > settings.releaseSeconds)
        releaseSeconds += (settings.slowReleaseSeconds - settings.releaseSeconds) * clampFloat(memory, 0.0f, 1.0f);

    const float seconds = target > gr ? settings.attackSeconds : releaseSeconds;
    const float coefficient = onePoleCoeff(sampleRate, std::max(seconds, 1.0e-6f));
    gr += coefficient * (target - gr);
    gr = flushDenormal(std::max(gr, 0.0f));

    // Program memory follows the signal, not the tail. Otherwise a release
    // would slow itself down while gain reduction is still high.
    if (detectorDb > settings.thresholdDb)
    {
        const float memoryTarget = clampFloat(gr / tunables::vmuMemoryReferenceDb, 0.0f, 1.0f);
        memory += memoryCoeff * (memoryTarget - memory);
        memory = flushDenormal(memory);
    }

    color = clampFloat(tunables::vmuColorPerInputDb * std::fabs(settings.inputDb)
                           + tunables::vmuColorPerGrDb * gr,
                       0.0f,
                       1.0f);

    return dbToGain(-gr + settings.outputDb);
}

void VariMuModel::writeTelemetry(VisualFrame& frame) const noexcept
{
    frame.detDb = detectorDb;
    frame.vmuHeatDb = detectorDb;
    frame.vmuRatioNow = ratio;
    frame.vmuGlow = clampFloat(gr / 18.0f, 0.0f, 1.0f);
}

bool VariMuModel::hasBadState() const noexcept
{
    return peak.hasBadState() || isBadFloat(gr) || isBadFloat(memory) || isBadFloat(color);
}

} // namespace squeeze
