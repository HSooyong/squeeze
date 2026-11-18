#include "VcaModel.h"

#include "MathUtil.h"
#include "StaticCurve.h"
#include "Tunables.h"

namespace squeeze
{

float VcaModel::attackDbPerSecond(float overDb) noexcept
{
    // 0.9 * over / time, so a constant slew reaches 90% of an infinite-ratio step on time.
    const float pointsOver[3] = { tunables::vcaAttackOverDb[0], tunables::vcaAttackOverDb[1], tunables::vcaAttackOverDb[2] };
    const float pointsRate[3] = {
        0.9f * pointsOver[0] / tunables::vcaAttackSeconds[0],
        0.9f * pointsOver[1] / tunables::vcaAttackSeconds[1],
        0.9f * pointsOver[2] / tunables::vcaAttackSeconds[2],
    };

    if (overDb <= 0.0f)
        return 0.0f;
    if (overDb <= pointsOver[0])
        return pointsRate[0] * (overDb / pointsOver[0]);
    if (overDb >= pointsOver[2])
        return pointsRate[2] * (overDb / pointsOver[2]);
    if (overDb <= pointsOver[1])
    {
        const float blend = (overDb - pointsOver[0]) / (pointsOver[1] - pointsOver[0]);
        return pointsRate[0] + blend * (pointsRate[1] - pointsRate[0]);
    }

    const float blend = (overDb - pointsOver[1]) / (pointsOver[2] - pointsOver[1]);
    return pointsRate[1] + blend * (pointsRate[2] - pointsRate[1]);
}

void VcaModel::prepare(double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    powerCoeff = onePoleCoeff(sampleRate, tunables::vcaRmsSeconds);
    reset();
}

void VcaModel::reset() noexcept
{
    power = 0.0f;
    gr = 0.0f;
    detectorDb = kDbFloor;
    overDb = 0.0f;
    attackRate = 0.0f;
    releasing = false;
}

void VcaModel::seedGr(float grDb) noexcept
{
    gr = grDb;
}

float VcaModel::process(float left, float right, int numChannels, const VcaSettings& settings) noexcept
{
    const float energy = numChannels > 1 ? (left * left + right * right) * 0.5f : left * left;
    power += powerCoeff * (energy - power);
    power = flushDenormal(std::max(power, 0.0f));

    detectorDb = powerToDb(power);
    overDb = detectorDb - settings.thresholdDb;
    const float target = -StaticCurve::vcaGainDb(detectorDb, settings.thresholdDb, settings.ratio);

    releasing = target < gr;
    const float rate = releasing ? tunables::vcaReleaseDbPerSecond : attackDbPerSecond(std::max(0.0f, overDb));
    attackRate = rate;
    const float step = static_cast<float>(rate / sampleRate);

    if (target > gr)
        gr = std::min(target, gr + step);
    else
        gr = std::max(target, gr - step);

    gr = flushDenormal(std::max(gr, 0.0f));
    return dbToGain(-gr + settings.outputDb);
}

void VcaModel::writeTelemetry(VisualFrame& frame) const noexcept
{
    frame.detDb = detectorDb;
    frame.vcaRmsDb = detectorDb;
    frame.vcaOverDb = overDb;
    frame.vcaValveOpen = dbToGain(-gr);
    frame.vcaAttackRateDbPerMs = attackRate / 1000.0f;
    frame.vcaReleasing = releasing ? 1.0f : 0.0f;
}

bool VcaModel::hasBadState() const noexcept
{
    return isBadFloat(power) || isBadFloat(gr);
}

} // namespace squeeze
