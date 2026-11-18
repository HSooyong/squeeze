#include "Engine.h"

#include "MathUtil.h"
#include "Tunables.h"

#include <algorithm>
#include <cmath>

namespace squeeze
{
namespace
{

float mixLog(float a, float b, float blend) noexcept
{
    if (blend <= 0.0f)
        return a;
    if (blend >= 1.0f)
        return b;
    return std::exp((1.0f - blend) * std::log(std::max(a, 1.0e-8f)) + blend * std::log(std::max(b, 1.0e-8f)));
}

} // namespace

void Engine::prepare(double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    channels = std::max(1, numChannels);
    fadeStep = 1.0f / static_cast<float>(tunables::modeCrossfadeSeconds * sampleRate);

    optoPeak.prepare(sampleRate, tunables::parameterSmoothSeconds);
    optoOutput.prepare(sampleRate, tunables::parameterSmoothSeconds);
    optoLimit.prepare(sampleRate, tunables::stepSmoothSeconds);
    vcaThreshold.prepare(sampleRate, tunables::parameterSmoothSeconds);
    vcaRatio.prepare(sampleRate, tunables::parameterSmoothSeconds);
    vcaOutput.prepare(sampleRate, tunables::parameterSmoothSeconds);
    fetInput.prepare(sampleRate, tunables::parameterSmoothSeconds);
    fetOutput.prepare(sampleRate, tunables::parameterSmoothSeconds);
    fetAttack.prepare(sampleRate, tunables::timeSmoothSeconds);
    fetRelease.prepare(sampleRate, tunables::timeSmoothSeconds);
    fetRatio.prepare(sampleRate, tunables::stepSmoothSeconds);
    vmuInput.prepare(sampleRate, tunables::parameterSmoothSeconds);
    vmuThreshold.prepare(sampleRate, tunables::parameterSmoothSeconds);
    vmuOutput.prepare(sampleRate, tunables::parameterSmoothSeconds);
    vmuTime.prepare(sampleRate, tunables::stepSmoothSeconds);

    opto.prepare(sampleRate);
    vca.prepare(sampleRate);
    fet.prepare(sampleRate);
    variMu.prepare(sampleRate);
    levelMatch.prepare(sampleRate, channels);
    reset();
}

void Engine::reset() noexcept
{
    sampleTime = 0;
    currentGr = 0.0f;
    currentDetector = kDbFloor;
    fading = false;
    fade = 1.0f;
    opto.reset();
    vca.reset();
    fet.reset();
    variMu.reset();
    levelMatch.reset();
}

void Engine::snapTo(const EngineTargets& targets) noexcept
{
    mode = targets.mode;
    fading = false;
    fade = 1.0f;
    optoPeak.snap(targets.optoPeakReduction);
    optoOutput.snap(targets.optoOutputDb);
    optoLimit.snap(targets.optoLimit);
    vcaThreshold.snap(targets.vcaThresholdDb);
    vcaRatio.snap(targets.vcaRatio);
    vcaOutput.snap(targets.vcaOutputDb);
    fetInput.snap(targets.fetInputDb);
    fetOutput.snap(targets.fetOutputDb);
    fetAttack.snap(targets.fetAttackSeconds);
    fetRelease.snap(targets.fetReleaseSeconds);
    fetRatio.snap(targets.fetRatioIndex);
    vmuInput.snap(targets.vmuInputDb);
    vmuThreshold.snap(targets.vmuThresholdDb);
    vmuOutput.snap(targets.vmuOutputDb);
    vmuTime.snap(targets.vmuTimeIndex);
    levelMatch.setEnabled(targets.levelMatch);
}

void Engine::advanceControls(const EngineTargets& targets) noexcept
{
    optoPeak.next(targets.optoPeakReduction);
    optoOutput.next(targets.optoOutputDb);
    optoLimit.next(clampFloat(targets.optoLimit, 0.0f, 1.0f));
    vcaThreshold.next(targets.vcaThresholdDb);
    vcaRatio.next(targets.vcaRatio);
    vcaOutput.next(targets.vcaOutputDb);
    fetInput.next(targets.fetInputDb);
    fetOutput.next(targets.fetOutputDb);
    fetAttack.next(targets.fetAttackSeconds);
    fetRelease.next(targets.fetReleaseSeconds);
    const float ratioLimit = targets.fetAllEnabled ? 4.0f : 3.0f;
    fetRatio.next(clampFloat(targets.fetRatioIndex, 0.0f, ratioLimit));
    vmuInput.next(targets.vmuInputDb);
    vmuThreshold.next(targets.vmuThresholdDb);
    vmuOutput.next(targets.vmuOutputDb);
    vmuTime.next(clampFloat(targets.vmuTimeIndex, 0.0f, 5.0f));
}

FetSettings Engine::fetSettings() const noexcept
{
    const float index = clampFloat(fetRatio.value, 0.0f, 4.0f);
    const int lower = std::min(3, static_cast<int>(index));
    const int upper = std::min(4, lower + 1);
    const float blend = index - static_cast<float>(lower);

    auto ratioOf = [](int step) noexcept {
        return step >= 4 ? tunables::fetAllRatio : tunables::fetRatios[step];
    };
    auto kneeOf = [](int step) noexcept {
        return step >= 4 ? tunables::fetAllKneeDb : tunables::fetKneeDb[step];
    };

    FetSettings settings;
    settings.inputDb = fetInput.value;
    settings.outputDb = fetOutput.value;
    settings.attackSeconds = std::max(fetAttack.value, 1.0e-6f);
    settings.releaseSeconds = std::max(fetRelease.value, 1.0e-4f);
    settings.ratio = ratioOf(lower) + (ratioOf(upper) - ratioOf(lower)) * blend;
    settings.kneeDb = kneeOf(lower) + (kneeOf(upper) - kneeOf(lower)) * blend;
    settings.allMode = index >= 3.5f;
    return settings;
}

VariMuSettings Engine::variMuSettings() const noexcept
{
    const float index = clampFloat(vmuTime.value, 0.0f, 5.0f);
    const int lower = std::min(4, static_cast<int>(index));
    const int upper = std::min(5, lower + 1);
    const float blend = index - static_cast<float>(lower);
    const auto& first = tunables::vmuTiming[lower];
    const auto& second = tunables::vmuTiming[upper];

    VariMuSettings settings;
    settings.inputDb = vmuInput.value;
    settings.thresholdDb = vmuThreshold.value;
    settings.outputDb = vmuOutput.value;
    settings.attackSeconds = mixLog(first.attackSeconds, second.attackSeconds, blend);
    settings.releaseSeconds = mixLog(first.releaseSeconds, second.releaseSeconds, blend);

    const float slowA = first.programDependent ? first.slowReleaseSeconds : first.releaseSeconds;
    const float slowB = second.programDependent ? second.slowReleaseSeconds : second.releaseSeconds;
    settings.slowReleaseSeconds = (first.programDependent || second.programDependent) ? mixLog(slowA, slowB, blend) : 0.0f;
    if (!(first.programDependent || second.programDependent))
        settings.slowReleaseSeconds = 0.0f;
    return settings;
}

Engine::Gains Engine::render(Mode rendered, float left, float right, int numChannels, std::int64_t sampleIndex) noexcept
{
    Gains gains;
    switch (rendered)
    {
        case Mode::opto:
        {
            OptoSettings settings;
            settings.peakReduction = optoPeak.value;
            settings.outputDb = optoOutput.value;
            settings.limitMix = optoLimit.value;
            gains.linear = opto.process(left, right, numChannels, settings);
            gains.grDb = opto.grDb();
            gains.color = 0.0f;
            break;
        }
        case Mode::vca:
        {
            VcaSettings settings;
            settings.thresholdDb = vcaThreshold.value;
            settings.ratio = std::max(vcaRatio.value, 1.0f);
            settings.outputDb = vcaOutput.value;
            gains.linear = vca.process(left, right, numChannels, settings);
            gains.grDb = vca.grDb();
            gains.color = 0.0f;
            break;
        }
        case Mode::fet:
        {
            const FetSettings settings = fetSettings();
            gains.linear = fet.process(left, right, numChannels, settings, sampleIndex);
            gains.grDb = fet.grDb();
            gains.color = fet.colorAmount();
            break;
        }
        case Mode::variMu:
        {
            const VariMuSettings settings = variMuSettings();
            gains.linear = variMu.process(left, right, numChannels, settings);
            gains.grDb = variMu.grDb();
            gains.color = variMu.colorAmount();
            break;
        }
    }

    return gains;
}

void Engine::applyLevelMatch(const float* const* dry,
                             float* const* wet,
                             int numChannels,
                             int numSamples,
                             bool enabled) noexcept
{
    levelMatch.setEnabled(enabled);
    if (!enabled || dry == nullptr || wet == nullptr)
        return;

    levelMatch.process(dry, wet, numChannels, numSamples);
}

void Engine::process(float* const* channels,
                     float* colorDrive,
                     int numChannels,
                     int numSamples,
                     const EngineTargets& targets,
                     VisualFrame& frame) noexcept
{
    if (channels == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    if (targets.mode != mode)
    {
        const float previousGr = currentGr;
        fadingFrom = mode;
        mode = targets.mode;
        fade = 0.0f;
        fading = true;
        switch (mode)
        {
            case Mode::opto: opto.seedGr(previousGr); break;
            case Mode::vca: vca.seedGr(previousGr); break;
            case Mode::fet: fet.seedGr(previousGr); break;
            case Mode::variMu: variMu.seedGr(previousGr); break;
        }
    }

    levelMatch.setEnabled(targets.levelMatch);
    const std::int64_t blockStart = sampleTime;
    float maxGr = 0.0f;
    Gains latest;

    for (int index = 0; index < numSamples; ++index)
    {
        advanceControls(targets);
        const float left = channels[0][index];
        const float right = numChannels > 1 ? channels[1][index] : left;

        latest = render(mode, left, right, numChannels, sampleTime);
        float gain = latest.linear;
        float color = latest.color;

        if (fading)
        {
            const Gains previous = render(fadingFrom, left, right, numChannels, sampleTime);
            const float angle = clampFloat(fade, 0.0f, 1.0f) * 1.57079632679f;
            const float oldWeight = std::cos(angle);
            const float newWeight = std::sin(angle);
            gain = oldWeight * previous.linear + newWeight * latest.linear;
            color = oldWeight * previous.color + newWeight * latest.color;
            fade += fadeStep;
            if (fade >= 1.0f)
                fading = false;
        }

        channels[0][index] = left * gain;
        if (numChannels > 1)
            channels[1][index] = right * gain;

        if (colorDrive != nullptr)
            colorDrive[index] = color;

        currentGr = latest.grDb;
        maxGr = std::max(maxGr, currentGr);
        ++sampleTime;
    }

    frame = {};
    frame.sampleTime = blockStart;
    frame.mode = static_cast<int>(mode);
    frame.grDb = currentGr;
    frame.grMaxDb = maxGr;
    frame.gainLin = latest.linear;
    switch (mode)
    {
        case Mode::opto:
            currentDetector = opto.grDb();
            opto.writeTelemetry(frame);
            currentDetector = frame.detDb;
            break;
        case Mode::vca:
            vca.writeTelemetry(frame);
            currentDetector = frame.detDb;
            break;
        case Mode::fet:
            fet.writeTelemetry(frame, blockStart);
            currentDetector = frame.detDb;
            break;
        case Mode::variMu:
            variMu.writeTelemetry(frame);
            currentDetector = frame.detDb;
            break;
    }

    frame.lmTrimDb = levelMatch.trimDb();
}

bool Engine::hasBadState() const noexcept
{
    return opto.hasBadState() || vca.hasBadState() || fet.hasBadState() || variMu.hasBadState() || levelMatch.hasBadState()
           || isBadFloat(currentGr);
}

} // namespace squeeze
