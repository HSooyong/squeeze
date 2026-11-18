#include "FetModel.h"

#include "StaticCurve.h"
#include "Tunables.h"

#include <algorithm>
#include <cmath>

namespace squeeze
{
namespace
{

float follow(float state, float target, double sampleRate, float seconds) noexcept
{
    const float coefficient = onePoleCoeff(sampleRate, std::max(seconds, 1.0e-6f));
    state += coefficient * (target - state);
    return flushDenormal(state);
}

} // namespace

void FetModel::prepare(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    peak.prepare(sampleRate, tunables::fetPeakHoldSeconds);
    reset();
}

void FetModel::reset() noexcept
{
    peak.reset();
    gr = 0.0f;
    previousGr = 0.0f;
    detectorDb = kDbFloor;
    drivenDb = 0.0f;
    overDb = 0.0f;
    color = 0.0f;
    releaseProgress = 0.0f;
    recentPeakGr = 0.0f;
    eventArmed = false;
    eventDone = false;
    crossSample = 0.0;
    grStartSample = 0.0;
    latchedTarget = 0.0f;
}

void FetModel::seedGr(float grDb) noexcept
{
    gr = grDb;
    previousGr = grDb;
    recentPeakGr = std::max(recentPeakGr, grDb);
}

float FetModel::process(float left, float right, int numChannels, const FetSettings& settings, std::int64_t sampleIndex) noexcept
{
    const float magnitude = numChannels > 1 ? std::max(std::fabs(left), std::fabs(right)) : std::fabs(left);
    const float held = peak.process(magnitude);
    drivenDb = linearToDb(held) + settings.inputDb;
    detectorDb = drivenDb;
    overDb = detectorDb - tunables::fetThresholdDb;

    float target = -StaticCurve::fetGainDb(detectorDb, settings.ratio, settings.kneeDb);

    const bool releasing = target < gr;
    if (settings.allMode && releasing)
    {
        // A short lead follower runs ahead of the main release and pulls GR past the target.
        const float lead = follow(gr, target, sampleRate, settings.releaseSeconds * 0.45f);
        const float body = follow(gr, target, sampleRate, settings.releaseSeconds);
        target = body + tunables::fetAllOvershoot * (lead - body);
    }

    previousGr = gr;
    const float seconds = target > gr ? settings.attackSeconds : settings.releaseSeconds;
    gr = follow(gr, target, sampleRate, seconds);
    gr = std::max(gr, -6.0f);
    recentPeakGr = std::max(gr, recentPeakGr * 0.9995f);

    if (recentPeakGr > 0.5f && gr < recentPeakGr)
        releaseProgress = clampFloat(1.0f - gr / recentPeakGr, 0.0f, 1.0f);
    else
        releaseProgress = 0.0f;

    const bool over = detectorDb > tunables::fetThresholdDb;
    if (over && !eventArmed)
    {
        eventArmed = true;
        eventDone = false;
        crossSample = static_cast<double>(sampleIndex);
        latchedTarget = std::max(target, 0.1f);
    }
    else if (!over && eventDone)
    {
        eventArmed = false;
    }

    if (eventArmed && !eventDone && latchedTarget > 0.1f)
    {
        const float mark = 0.63f * latchedTarget;
        if (previousGr < mark && gr >= mark && gr > previousGr)
        {
            const float fraction = (mark - previousGr) / (gr - previousGr);
            // sampleIndex is the update that just crossed the mark, so the
            // fractional position is this update plus the fraction through it.
            grStartSample = static_cast<double>(sampleIndex) + static_cast<double>(fraction);
            eventDone = true;
        }
    }

    color = clampFloat(tunables::fetColorPerInputDb * std::max(0.0f, settings.inputDb)
                           + tunables::fetColorPerGrDb * std::max(0.0f, gr),
                       0.0f,
                       1.0f);

    return dbToGain(-gr + settings.outputDb);
}

void FetModel::writeTelemetry(VisualFrame& frame, std::int64_t blockStartSample) const noexcept
{
    frame.detDb = detectorDb;
    frame.fetDrivenDb = drivenDb;
    frame.fetOverDb = overDb;
    frame.fetArc = clampFloat(gr / 24.0f, 0.0f, 1.0f);
    frame.fetEventValid = eventDone ? 1.0f : 0.0f;
    frame.fetCrossOffset = static_cast<float>(crossSample - static_cast<double>(blockStartSample));
    frame.fetGrStartOffset = static_cast<float>(grStartSample - static_cast<double>(blockStartSample));
    frame.fetReleaseProgress = releaseProgress;
}

bool FetModel::hasBadState() const noexcept
{
    return peak.hasBadState() || isBadFloat(gr) || isBadFloat(color);
}

} // namespace squeeze
