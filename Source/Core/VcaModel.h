#pragma once

#include "VisualFrame.h"

namespace squeeze
{

struct VcaSettings
{
    float thresholdDb = -18.0f;
    float ratio = 4.0f;
    float outputDb = 0.0f;
};

class VcaModel
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void seedGr(float grDb) noexcept;

    float process(float left, float right, int numChannels, const VcaSettings& settings) noexcept;

    float grDb() const noexcept { return gr; }
    float colorAmount() const noexcept { return 0.0f; }
    void writeTelemetry(VisualFrame& frame) const noexcept;
    bool hasBadState() const noexcept;

    // Slew used for the current overshoot, in dB per second. Public for the rate test.
    static float attackDbPerSecond(float overDb) noexcept;

private:
    float powerCoeff = 1.0f;
    double sampleRate = 44100.0;
    float power = 0.0f;
    float gr = 0.0f;
    float detectorDb = -160.0f;
    float overDb = 0.0f;
    float attackRate = 0.0f;
    bool releasing = false;
};

} // namespace squeeze
