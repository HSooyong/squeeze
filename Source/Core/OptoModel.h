#pragma once

#include "VisualFrame.h"

namespace squeeze
{

struct OptoSettings
{
    float peakReduction = 40.0f;
    float outputDb = 0.0f;
    float limitMix = 0.0f;
};

class OptoModel
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void seedGr(float grDb) noexcept;

    // Returns the linear gain, including the output gain. Updates the optical state.
    float process(float left, float right, int numChannels, const OptoSettings& settings) noexcept;

    float grDb() const noexcept { return gr; }
    float colorAmount() const noexcept { return 0.0f; }
    void writeTelemetry(VisualFrame& frame) const noexcept;
    bool hasBadState() const noexcept;

private:
    float detectorCoeff = 1.0f;
    float attackCoeff = 1.0f;
    float fastReleaseCoeff = 1.0f;
    float memoryCoeff = 1.0f;
    double sampleRate = 44100.0;

    float envelope = 0.0f;
    float fast = 0.0f;
    float slow = 0.0f;
    float memory = 0.0f;
    float gr = 0.0f;
    float detectorDb = -160.0f;
};

} // namespace squeeze
