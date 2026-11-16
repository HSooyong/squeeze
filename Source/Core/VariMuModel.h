#pragma once

#include "MathUtil.h"
#include "VisualFrame.h"

namespace squeeze
{

struct VariMuSettings
{
    float inputDb = 0.0f;
    float thresholdDb = -22.0f;
    float outputDb = 0.0f;
    float attackSeconds = 0.0004f;
    float releaseSeconds = 2.0f;
    float slowReleaseSeconds = 0.0f;
};

class VariMuModel
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void seedGr(float grDb) noexcept;

    float process(float left, float right, int numChannels, const VariMuSettings& settings) noexcept;

    float grDb() const noexcept { return gr; }
    float colorAmount() const noexcept { return color; }
    float ratioNow() const noexcept { return ratio; }
    void writeTelemetry(VisualFrame& frame) const noexcept;
    bool hasBadState() const noexcept;

private:
    PeakHold peak;
    double sampleRate = 44100.0;
    float memoryCoeff = 1.0f;
    float gr = 0.0f;
    float memory = 0.0f;
    float detectorDb = -160.0f;
    float ratio = 1.0f;
    float color = 0.0f;
};

} // namespace squeeze
