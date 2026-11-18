#pragma once

#include "Parameters.h"

namespace squeeze::ui
{

// Four listening setups per mode. Names stay generic.
struct ScenePreset
{
    const char* name;
    const char* ids[6];
    float values[6];
    int count;
};

inline const ScenePreset* scenesForMode(int mode, int& count)
{
    static const ScenePreset lamp[] = {
        { "Vocal gentle", { params::optoPeakRed, params::optoLimit, params::optoGain }, { 30.0f, 0.0f, 0.0f }, 3 },
        { "Vocal tight", { params::optoPeakRed, params::optoLimit, params::optoGain }, { 62.0f, 0.0f, 0.0f }, 3 },
        { "Bass smooth", { params::optoPeakRed, params::optoLimit, params::optoGain }, { 50.0f, 0.0f, 0.0f }, 3 },
        { "Acoustic guitar", { params::optoPeakRed, params::optoLimit, params::optoGain }, { 40.0f, 0.0f, 0.0f }, 3 },
    };
    static const ScenePreset needle[] = {
        { "Drum punch", { params::vcaThreshold, params::vcaRatio, params::vcaOutput }, { -20.0f, 4.0f, 0.0f }, 3 },
        { "Vocal steady", { params::vcaThreshold, params::vcaRatio, params::vcaOutput }, { -18.0f, 4.0f, 0.0f }, 3 },
        { "Bass even", { params::vcaThreshold, params::vcaRatio, params::vcaOutput }, { -24.0f, 3.0f, 0.0f }, 3 },
        { "Mix bus", { params::vcaThreshold, params::vcaRatio, params::vcaOutput }, { -12.0f, 2.0f, 0.0f }, 3 },
    };
    static const ScenePreset spark[] = {
        { "Drum smash", { params::fetInput, params::fetRatio, params::fetAttack, params::fetRelease }, { 40.0f, 3.0f, 200.0f, 100.0f }, 4 },
        { "Vocal forward", { params::fetInput, params::fetRatio, params::fetAttack, params::fetRelease }, { 34.0f, 0.0f, 300.0f, 250.0f }, 4 },
        { "Snare snap", { params::fetInput, params::fetRatio, params::fetAttack, params::fetRelease }, { 30.0f, 1.0f, 800.0f, 50.0f }, 4 },
        { "Room crush", { params::fetInput, params::fetRatio, params::fetAttack, params::fetRelease }, { 44.0f, 3.0f, 20.0f, 1100.0f }, 4 },
    };
    static const ScenePreset tube[] = {
        { "Mix glue", { params::vmuThreshold, params::vmuTime, params::vmuInput }, { -16.0f, 4.0f, 0.0f }, 3 },
        { "Vocal warm", { params::vmuThreshold, params::vmuTime, params::vmuInput }, { -22.0f, 2.0f, 0.0f }, 3 },
        { "Piano soft", { params::vmuThreshold, params::vmuTime, params::vmuInput }, { -20.0f, 3.0f, 0.0f }, 3 },
        { "Master gentle", { params::vmuThreshold, params::vmuTime, params::vmuInput }, { -12.0f, 5.0f, 0.0f }, 3 },
    };

    count = 4;

    switch (mode)
    {
        case 0: return lamp;
        case 1: return needle;
        case 2: return spark;
        default: return tube;
    }
}

} // namespace squeeze::ui
