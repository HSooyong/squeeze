#pragma once

#include "MathUtil.h"
#include "VisualFrame.h"

namespace squeeze
{

struct FetSettings
{
    float inputDb = 0.0f;
    float outputDb = 0.0f;
    float attackSeconds = 300.0e-6f;
    float releaseSeconds = 0.250f;
    float ratio = 4.0f;
    float kneeDb = 6.0f;
    bool allMode = false;
};

class FetModel
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void seedGr(float grDb) noexcept;

    float process(float left, float right, int numChannels, const FetSettings& settings, std::int64_t sampleIndex) noexcept;

    float grDb() const noexcept { return gr; }
    float colorAmount() const noexcept { return color; }
    void writeTelemetry(VisualFrame& frame, std::int64_t blockStartSample) const noexcept;
    bool hasBadState() const noexcept;

private:
    PeakHold peak;
    double sampleRate = 44100.0;
    float gr = 0.0f;
    float detectorDb = -160.0f;
    float drivenDb = 0.0f;
    float overDb = 0.0f;
    float color = 0.0f;
    float releaseProgress = 0.0f;
    float recentPeakGr = 0.0f;

    bool eventArmed = false;
    bool eventDone = false;
    double crossSample = 0.0;
    double grStartSample = 0.0;
    float latchedTarget = 0.0f;
    float previousGr = 0.0f;
};

} // namespace squeeze
