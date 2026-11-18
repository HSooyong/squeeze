#pragma once

#include "FetModel.h"
#include "LevelMatch.h"
#include "Mode.h"
#include "OptoModel.h"
#include "VariMuModel.h"
#include "VcaModel.h"
#include "VisualFrame.h"

#include <cstdint>

namespace squeeze
{

struct EngineTargets
{
    Mode mode = Mode::opto;
    bool levelMatch = true;
    float optoPeakReduction = 40.0f;
    float optoOutputDb = 0.0f;
    float optoLimit = 0.0f;
    float vcaThresholdDb = -18.0f;
    float vcaRatio = 4.0f;
    float vcaOutputDb = 0.0f;
    float fetInputDb = 0.0f;
    float fetOutputDb = 0.0f;
    float fetAttackSeconds = 300.0e-6f;
    float fetReleaseSeconds = 0.250f;
    float fetRatioIndex = 0.0f;
    bool fetAllEnabled = false;
    float vmuInputDb = 0.0f;
    float vmuThresholdDb = -22.0f;
    float vmuOutputDb = 0.0f;
    float vmuTimeIndex = 2.0f;
};

class Engine
{
public:
    void prepare(double sampleRate, int numChannels);
    void reset() noexcept;
    void snapTo(const EngineTargets& targets) noexcept;

    // Applies the linked gain in place and writes one color amount per sample.
    void process(float* const* channels,
                 float* colorDrive,
                 int numChannels,
                 int numSamples,
                 const EngineTargets& targets,
                 VisualFrame& frame) noexcept;

    void applyLevelMatch(const float* const* dry,
                         float* const* wet,
                         int numChannels,
                         int numSamples,
                         bool enabled) noexcept;

    float currentGrDb() const noexcept { return currentGr; }
    float currentDetectorDb() const noexcept { return currentDetector; }
    float levelMatchTrimDb() const noexcept { return levelMatch.trimDb(); }
    bool hasBadState() const noexcept;

private:
    struct Gains
    {
        float linear = 1.0f;
        float color = 0.0f;
        float grDb = 0.0f;
    };

    Gains render(Mode rendered, float left, float right, int numChannels, std::int64_t sampleIndex) noexcept;
    void advanceControls(const EngineTargets& targets) noexcept;
    FetSettings fetSettings() const noexcept;
    VariMuSettings variMuSettings() const noexcept;

    double sampleRate = 44100.0;
    int channels = 2;
    std::int64_t sampleTime = 0;
    float currentGr = 0.0f;
    float currentDetector = -160.0f;

    Mode mode = Mode::opto;
    Mode fadingFrom = Mode::opto;
    bool fading = false;
    float fade = 1.0f;
    float fadeStep = 1.0f;

    Ramp optoPeak;
    Ramp optoOutput;
    Ramp optoLimit;
    Ramp vcaThreshold;
    Ramp vcaRatio;
    Ramp vcaOutput;
    Ramp fetInput;
    Ramp fetOutput;
    Ramp fetAttack;
    Ramp fetRelease;
    Ramp fetRatio;
    Ramp vmuInput;
    Ramp vmuThreshold;
    Ramp vmuOutput;
    Ramp vmuTime;

    OptoModel opto;
    VcaModel vca;
    FetModel fet;
    VariMuModel variMu;
    LevelMatch levelMatch;
};

} // namespace squeeze
