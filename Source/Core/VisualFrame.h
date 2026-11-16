#pragma once

#include <cstdint>
#include <type_traits>

namespace squeeze
{

// One block of pictures for the UI. Fixed size, written on the audio thread.
// Mode selects which of the per-model fields are meaningful.
struct VisualFrame
{
    std::int64_t sampleTime = 0;
    int mode = 0;
    float inDb = -160.0f;
    float outDb = -160.0f;
    float detDb = -160.0f;
    float grDb = 0.0f;
    float grMaxDb = 0.0f;
    float gainLin = 1.0f;
    float lmTrimDb = 0.0f;

    float optoCell = 0.0f;
    float optoFast = 0.0f;
    float optoSlow = 0.0f;
    float optoMemory = 0.0f;

    float vcaRmsDb = -160.0f;
    float vcaOverDb = 0.0f;
    float vcaValveOpen = 1.0f;
    float vcaAttackRateDbPerMs = 0.0f;
    float vcaReleasing = 0.0f;

    float fetDrivenDb = 0.0f;
    float fetOverDb = 0.0f;
    float fetArc = 0.0f;
    float fetCrossOffset = 0.0f;
    float fetGrStartOffset = 0.0f;
    float fetEventValid = 0.0f;
    float fetReleaseProgress = 0.0f;
    float fetGrit = 0.0f;

    float vmuHeatDb = -160.0f;
    float vmuRatioNow = 1.0f;
    float vmuGlow = 0.0f;
    float vmuHarmonics = 0.0f;
};

static_assert(std::is_trivially_copyable<VisualFrame>::value, "VisualFrame stays a plain frame");

} // namespace squeeze
