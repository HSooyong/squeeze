#include "OptoModel.h"

#include "MathUtil.h"
#include "StaticCurve.h"
#include "Tunables.h"

namespace squeeze
{

void OptoModel::prepare(double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    detectorCoeff = onePoleCoeff(sampleRate, tunables::optoDetectorSeconds);
    attackCoeff = onePoleCoeff(sampleRate, tunables::optoAttackSeconds);
    fastReleaseCoeff = onePoleCoeff(sampleRate, tunables::optoFastReleaseSeconds);
    memoryCoeff = onePoleCoeff(sampleRate, tunables::optoMemorySeconds);
    reset();
}

void OptoModel::reset() noexcept
{
    envelope = 0.0f;
    fast = 0.0f;
    slow = 0.0f;
    memory = 0.0f;
    gr = 0.0f;
    detectorDb = kDbFloor;
}

void OptoModel::seedGr(float grDb) noexcept
{
    fast = grDb;
    slow = grDb;
    gr = grDb;
}

float OptoModel::process(float left, float right, int numChannels, const OptoSettings& settings) noexcept
{
    const float rectified = numChannels > 1 ? std::max(std::fabs(left), std::fabs(right)) : std::fabs(left);
    envelope += detectorCoeff * (rectified - envelope);
    envelope = flushDenormal(envelope);

    detectorDb = linearToDb(envelope) + tunables::optoSidechainDb(settings.peakReduction);
    const float target = -StaticCurve::optoGainDb(detectorDb, settings.limitMix);

    const float slowSeconds = tunables::optoSlowReleaseMinSeconds
                              + tunables::optoSlowReleaseSpanSeconds * clampFloat(memory, 0.0f, 1.0f);
    const float slowCoeff = target > slow ? attackCoeff : onePoleCoeff(sampleRate, slowSeconds);
    const float fastCoeff = target > fast ? attackCoeff : fastReleaseCoeff;

    fast += fastCoeff * (target - fast);
    slow += slowCoeff * (target - slow);
    fast = flushDenormal(fast);
    slow = flushDenormal(slow);
    gr = 0.5f * fast + 0.5f * slow;

    const float memoryTarget = clampFloat(gr / tunables::optoMemoryReferenceDb, 0.0f, 1.0f);
    memory += memoryCoeff * (memoryTarget - memory);
    memory = flushDenormal(memory);

    return dbToGain(-gr + settings.outputDb);
}

void OptoModel::writeTelemetry(VisualFrame& frame) const noexcept
{
    frame.detDb = detectorDb;
    frame.optoCell = clampFloat((detectorDb - (tunables::optoThresholdDb - 24.0f)) / 36.0f, 0.0f, 1.0f);
    frame.optoFast = fast;
    frame.optoSlow = slow;
    frame.optoMemory = memory;
}

bool OptoModel::hasBadState() const noexcept
{
    return isBadFloat(envelope) || isBadFloat(fast) || isBadFloat(slow) || isBadFloat(memory) || isBadFloat(gr);
}

} // namespace squeeze
